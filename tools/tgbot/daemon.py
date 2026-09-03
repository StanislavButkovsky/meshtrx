#!/usr/bin/env python3
"""Демон бота @meshtrx_bot: единственный, кто разговаривает с Telegram.

Забирает сообщения группы, складывает их в общую базу, отправляет то, что
поставили в очередь, и отвечает на команды. Длинный опрос Telegram отдаёт
обновление ровно одному клиенту, поэтому второго такого процесса быть не
должно — MCP-сервер работает через базу, а не через сеть.

Запуск:
    tools/tgbot/daemon.py            # обычная работа
    tools/tgbot/daemon.py --chats    # показать, из каких чатов приходят
                                      # сообщения, и их идентификаторы
"""

from __future__ import annotations

import argparse
import base64
import fcntl
import os
import socket
import ssl
import sys
import time
import urllib.parse
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import httpx                       # noqa: E402
import store                       # noqa: E402
from docs_index import DocsIndex   # noqa: E402

ENV_FILE = Path.home() / ".config" / "meshtrx" / "telegram.env"
LOCK_FILE = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share")) \
    / "meshtrx" / "daemon.lock"
API_HOST = "api.telegram.org"
API = "https://{host}/bot{token}/{method}"
POLL_TIMEOUT = 25                  # секунд держим длинный опрос
MAX_NET_FAILURES = 20              # подряд — считаем, что путь до Telegram пропал
DOCS_PULL_INTERVAL = 15 * 60       # как часто подтягиваем документацию из репозитория

# Адреса Telegram на случай, когда обращаться приходится по адресу, а не по
# имени. Список тот же, что в pick_telegram_route.py: системный резолв на
# сервере отдаёт нерабочий вариант, ради которого всё и затевалось.
ENDPOINT_IPS = [
    "149.154.167.220", "149.154.167.51", "149.154.175.50",
    "149.154.171.5", "149.154.175.100", "91.108.56.130", "149.154.166.110",
]


def acquire_lock():
    """Второй демон отбирал бы обновления у первого: длинный опрос отдаёт
    каждое ровно одному клиенту, и часть переписки пропадала бы молча. Поэтому
    единственность — не пожелание, а условие работы.

    Блокировка файла, а не поиск по списку процессов: она снимается сама, когда
    процесс умирает, в том числе от kill -9 и при перезагрузке."""
    LOCK_FILE.parent.mkdir(parents=True, exist_ok=True)
    handle = open(LOCK_FILE, "w")
    try:
        fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        return None
    handle.write(f"{os.getpid()}\n")
    handle.flush()
    return handle   # держим открытым до конца жизни процесса


def load_env() -> dict:
    """Токен и настройки лежат вне репозитория: он публичный, а токен бота —
    это полный доступ к нему."""
    values = {}
    if ENV_FILE.exists():
        for line in ENV_FILE.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, val = line.split("=", 1)
            values[key.strip()] = val.strip().strip('"').strip("'")
    for key in ("MESHTRX_TG_BOT_TOKEN", "MESHTRX_TG_CHAT_ID", "MESHTRX_TG_PROXY"):
        if os.environ.get(key):
            values[key] = os.environ[key]
    return values


def _tls_without_sni() -> ssl.SSLContext:
    """Проверка цепочки остаётся, проверка имени — нет.

    Так приходится делать, когда до Telegram мы идём по адресу, а не по имени:
    имя в TLS-приветствии (SNI) — ровно то, по чему нас и отсекают, а для
    адреса SNI не отправляется вовсе и соединение проходит. Имя при этом
    проверяется, но один раз и вручную — при старте, в check_endpoint(): там
    мы смотрим, что в сертификате действительно значится api.telegram.org.
    Без этой проверки прокси мог бы подсунуть свой сертификат.
    """
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    return ctx


def check_endpoint(ip: str, proxy: str | None) -> bool:
    """Убедиться, что по этому адресу отвечает именно Telegram: сертификат
    выдан на api.telegram.org и подписан известным центром."""
    try:
        if proxy:
            parsed = urllib.parse.urlparse(proxy)
            raw = socket.create_connection((parsed.hostname, parsed.port), timeout=8)
            req = f"CONNECT {ip}:443 HTTP/1.1\r\nHost: {ip}:443\r\n"
            if parsed.username:
                token = base64.b64encode(
                    f"{parsed.username}:{parsed.password or ''}".encode()).decode()
                req += f"Proxy-Authorization: Basic {token}\r\n"
            raw.sendall((req + "\r\n").encode())
            answer = raw.recv(256)
            if b" 200 " not in answer.split(b"\r\n")[0]:
                raw.close()
                return False
        else:
            raw = socket.create_connection((ip, 443), timeout=8)

        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        with ctx.wrap_socket(raw) as tls:
            cert = tls.getpeercert()
        names = {v for k, v in cert.get("subjectAltName", ()) if k == "DNS"}
        # Telegram отдаёт wildcard, поэтому сравниваем и его
        return API_HOST in names or "*.telegram.org" in names
    except OSError:
        return False


class Bot:
    def __init__(self, token: str, allowed_chats: set[int],
                 proxy: str | None = None, host: str | None = None):
        self.token = token
        self.allowed = allowed_chats
        self.proxy = proxy
        # Куда обращаться: по имени (обычный случай) или по адресу — когда имя
        # в TLS-приветствии режет фильтрация по пути.
        self.host = host or API_HOST
        self.by_ip = self.host != API_HOST
        # Установка соединения и чтение разведены намеренно. Длинный опрос
        # честно висит полминуты и это норма, а вот первое соединение после
        # паузы на этом сервере иногда пропадает бесследно: TCP не встаёт
        # вовсе, зато следующая попытка проходит за 0,15 с. С общим таймаутом
        # каждый такой случай стоил бы полминуты тишины.
        self.http = httpx.Client(
            timeout=httpx.Timeout(connect=6.0, read=POLL_TIMEOUT + 10,
                                  write=15.0, pool=10.0),
            proxy=self.proxy or None,
            verify=_tls_without_sni() if self.by_ip else True,
            # Обращаясь по адресу, имя сервера сообщаем заголовком: без него
            # Telegram не поймёт, чей это запрос, и ответит редиректом.
            headers={"Host": API_HOST} if self.by_ip else None)
        self.conn = store.connect()
        self.docs = DocsIndex()
        self.net_failures = 0

    # ---------------------------------------------------------------- сеть
    def call(self, method: str, **params):
        try:
            r = self.http.post(
                API.format(host=self.host, token=self.token, method=method),
                json=params)
            data = r.json()
            if not data.get("ok"):
                print(f"[tg] {method}: {data.get('description')}", flush=True)
            self.net_failures = 0
            return data
        except Exception as e:                                   # noqa: BLE001
            # Это отказ сети, а не отказ Telegram: имя не разрешилось или
            # соединение оборвали по пути. На сервере, где часть адресов
            # Telegram отфильтрована, так выглядит пропавший маршрут — и молча
            # ждать тут нечего, иначе бот «сломан» без единой понятной строки.
            self.net_failures += 1
            print(f"[tg] {method} не удался ({self.net_failures}): {e}", flush=True)
            if self.net_failures >= MAX_NET_FAILURES:
                print("[tg] связь с Telegram потеряна — выхожу, пусть сервис "
                      "поднимут заново и переберут маршрут", flush=True)
                raise SystemExit(75)          # EX_TEMPFAIL: systemd перезапустит
            time.sleep(min(self.net_failures, 10))
            return {"ok": False}

    def send(self, chat_id: int, text: str, reply_to: int | None = None) -> bool:
        # Telegram режет сообщения длиннее 4096 символов, поэтому длинный ответ
        # разбиваем сами — иначе он не дойдёт вовсе.
        chunks = [text[i:i + 3900] for i in range(0, len(text), 3900)] or [""]
        ok = True
        for i, chunk in enumerate(chunks):
            params = {"chat_id": chat_id, "text": chunk,
                      "disable_web_page_preview": True}
            if reply_to and i == 0:
                params["reply_to_message_id"] = reply_to
            ok = self.call("sendMessage", **params).get("ok", False) and ok
        return ok

    # ---------------------------------------------------------------- команды
    def handle_command(self, cmd: str, args: str, msg: dict, chat: dict, user: dict):
        """Служебные ответы уходят без цитаты, ответы по существу — с цитатой.

        Причина в том, как Telegram показывает ответ на удалённое сообщение:
        вместо команды остаётся плашка «Удалённое сообщение», а под ней висит
        наш ответ. Команды люди удаляют часто — они мусор в переписке, — и чат
        зарастает такими обрубками. Цитата остаётся там, где она несёт смысл:
        в ответе на заданный вопрос, чтобы через сотню сообщений было понятно,
        о чём речь.
        """
        chat_id = chat["id"]
        name = user.get("username") or user.get("first_name") or "?"

        if cmd == "status":
            info = self.docs.status()
            waiting = len(store.open_questions(self.conn))
            tail = f", жду ответа на {waiting}" if waiting else ""
            self.send(chat_id,
                      f"Читаю {info['files']} файлов документации "
                      f"({info['sections']} разделов), обновлял {info['updated']}{tail}.")
            return

        if cmd == "reload":
            changed, rev = self.docs.pull()
            info = self.docs.rebuild()
            tail = f" (версия {rev})" if changed else ""
            self.send(chat_id,
                      f"Перечитал документацию: {info['files']} файлов, "
                      f"{info['sections']} разделов{tail}.")
            return

        if cmd == "ask":
            question = args.strip()
            if not question:
                self.send(chat_id, "Спросите что-нибудь после команды, "
                                   "например: /ask сколько можно говорить")
                return

            # Отвечать сам бот не пробует. Поиск по словам находит похожий по
            # словам раздел, а не ответ на вопрос: «как подключить
            # ретранслятор» приводило к разделу про вызовы. Уверенный неверный
            # ответ дороже ожидания, поэтому вопрос копится в очереди, а
            # отвечает на него человек или агент — по-прежнему через этого же
            # бота, так что для спрашивающего ничего не меняется, кроме того,
            # что ответ приходит не мгновенно и по делу.
            store.add_question(self.conn, msg["message_id"], chat_id, name, question)
            self.send(chat_id, "Записал, отвечу.", msg["message_id"])
            return

    # ---------------------------------------------------------------- цикл
    def process_update(self, upd: dict, only_list_chats: bool = False):
        msg = upd.get("message") or upd.get("edited_message")
        if not msg:
            return
        chat = msg.get("chat", {})
        user = msg.get("from", {})
        text = msg.get("text", "") or ""

        if only_list_chats:
            print(f"чат {chat.get('id')}  «{chat.get('title') or chat.get('username')}»  "
                  f"тип {chat.get('type')}  от {user.get('username')}: {text[:40]}",
                  flush=True)
            return

        # Записываем сообщения из любого чата: иначе новая группа не обнаружит
        # себя сама и её идентификатор пришлось бы выискивать руками. Отвечать
        # и выполнять команды — только там, где разрешено.
        is_command = text.startswith("/")
        store.save_message(self.conn, msg, chat, user, is_command)

        if self.allowed and chat.get("id") not in self.allowed:
            print(f"[tg] чат {chat.get('id')} «{chat.get('title') or chat.get('username')}»"
                  f" ({chat.get('type')}) — записал, но отвечать там не разрешено",
                  flush=True)
            return

        if is_command:
            head, _, args = text.partition(" ")
            cmd = head[1:].split("@")[0].lower()   # «/ask@meshtrx_bot» → «ask»
            self.handle_command(cmd, args, msg, chat, user)

    def flush_outbox(self):
        for row in store.pending_outbox(self.conn):
            ok = self.send(row["chat_id"], row["text"], row["reply_to"])
            store.mark_sent(self.conn, row["id"], None if ok else "не отправлено")

    def run(self, only_list_chats: bool = False):
        offset = store.get_state(self.conn, "offset", 0)
        # Три попытки: первое обращение после простоя здесь регулярно пропадает,
        # а строка «бот @? на связи» в журнале выглядит как поломка, хотя бот
        # при этом работает.
        me = {}
        for _ in range(3):
            me = self.call("getMe").get("result", {})
            if me:
                break
        print(f"[tg] бот @{me.get('username', '?')} на связи; "
              f"чаты: {sorted(self.allowed) or 'любые'}", flush=True)

        next_pull = time.monotonic() + DOCS_PULL_INTERVAL
        while True:
            data = self.call("getUpdates", offset=offset, timeout=POLL_TIMEOUT)
            for upd in data.get("result", []):
                offset = upd["update_id"] + 1
                try:
                    self.process_update(upd, only_list_chats)
                except Exception as e:                           # noqa: BLE001
                    print(f"[tg] сбой обработки: {e}", flush=True)
            store.set_state(self.conn, "offset", offset)
            if not only_list_chats:
                self.flush_outbox()
                # Документация подтягивается сама: иначе бот отвечал бы по
                # состоянию на момент запуска, а замечают такое обычно после
                # того, как человек уже сделал по устаревшему совету.
                if time.monotonic() >= next_pull:
                    next_pull = time.monotonic() + DOCS_PULL_INTERVAL
                    changed, rev = self.docs.pull()
                    if changed:
                        info = self.docs.rebuild()
                        print(f"[tg] документация обновлена до {rev}: "
                              f"{info['sections']} разделов", flush=True)
            time.sleep(0.2)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--chats", action="store_true",
                    help="показать идентификаторы чатов и выйти по Ctrl+C")
    args = ap.parse_args()

    lock = acquire_lock()
    if lock is None:
        print("демон уже работает — второй не нужен", file=sys.stderr)
        return 0

    env = load_env()
    token = env.get("MESHTRX_TG_BOT_TOKEN")
    if not token:
        print(f"нет токена: положите его в {ENV_FILE} строкой "
              f"MESHTRX_TG_BOT_TOKEN=…", file=sys.stderr)
        return 1

    allowed = set()
    for part in (env.get("MESHTRX_TG_CHAT_ID") or "").replace(";", ",").split(","):
        part = part.strip()
        if part:
            allowed.add(int(part))

    proxy = env.get("MESHTRX_TG_PROXY") or None
    host = None
    if proxy:
        # Через прокси имя api.telegram.org в TLS-приветствии отсекается по
        # пути: соединение открывается и молча висит. По адресу того же сервера
        # запрос проходит, поэтому ищем адрес, за которым отвечает настоящий
        # Telegram — проверяя это по сертификату, а не по факту соединения.
        for ip in ENDPOINT_IPS:
            if check_endpoint(ip, proxy):
                host = ip
                print(f"[tg] через прокси, адрес {ip}", flush=True)
                break
        if host is None:
            print("[tg] через прокси не отвечает ни один адрес Telegram",
                  file=sys.stderr)
            return 1

    bot = Bot(token, set() if args.chats else allowed, proxy=proxy, host=host)
    try:
        bot.run(only_list_chats=args.chats)
    except KeyboardInterrupt:
        print("\nостановлено", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
