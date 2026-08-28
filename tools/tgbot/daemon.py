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
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import httpx                       # noqa: E402
import store                       # noqa: E402
from docs_index import DocsIndex   # noqa: E402

ENV_FILE = Path.home() / ".config" / "meshtrx" / "telegram.env"
API = "https://api.telegram.org/bot{token}/{method}"
POLL_TIMEOUT = 25                  # секунд держим длинный опрос


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
    for key in ("MESHTRX_TG_BOT_TOKEN", "MESHTRX_TG_CHAT_ID"):
        if os.environ.get(key):
            values[key] = os.environ[key]
    return values


class Bot:
    def __init__(self, token: str, allowed_chats: set[int]):
        self.token = token
        self.allowed = allowed_chats
        self.http = httpx.Client(timeout=POLL_TIMEOUT + 10)
        self.conn = store.connect()
        self.docs = DocsIndex()

    # ---------------------------------------------------------------- сеть
    def call(self, method: str, **params):
        try:
            r = self.http.post(API.format(token=self.token, method=method), json=params)
            data = r.json()
            if not data.get("ok"):
                print(f"[tg] {method}: {data.get('description')}", flush=True)
            return data
        except Exception as e:                                   # noqa: BLE001
            print(f"[tg] {method} не удался: {e}", flush=True)
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
        chat_id = chat["id"]
        name = user.get("username") or user.get("first_name") or "?"

        if cmd == "status":
            info = self.docs.status()
            waiting = len(store.open_questions(self.conn))
            tail = f", жду ответа на {waiting}" if waiting else ""
            self.send(chat_id,
                      f"Читаю {info['files']} файлов документации "
                      f"({info['sections']} разделов), обновлял {info['updated']}{tail}.",
                      msg["message_id"])
            return

        if cmd == "reload":
            info = self.docs.rebuild()
            self.send(chat_id,
                      f"Перечитал документацию: {info['files']} файлов, "
                      f"{info['sections']} разделов.", msg["message_id"])
            return

        if cmd == "ask":
            question = args.strip()
            if not question:
                self.send(chat_id, "Спросите что-нибудь после команды, "
                                   "например: /ask сколько можно говорить",
                          msg["message_id"])
                return
            qid = store.add_question(self.conn, msg["message_id"], chat_id, name, question)
            # Быстрый ответ по документации — если ничего похожего не нашлось,
            # вопрос остаётся в очереди и на него отвечает человек или агент.
            found = self.docs.search(question)
            if found:
                self.send(chat_id, found, msg["message_id"])
                store.close_question(self.conn, qid, found)
            else:
                self.send(chat_id, "Записал, разберусь и отвечу.",
                          msg["message_id"])
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
        me = self.call("getMe").get("result", {})
        print(f"[tg] бот @{me.get('username', '?')} на связи; "
              f"чаты: {sorted(self.allowed) or 'любые'}", flush=True)

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
            time.sleep(0.2)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--chats", action="store_true",
                    help="показать идентификаторы чатов и выйти по Ctrl+C")
    args = ap.parse_args()

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

    bot = Bot(token, set() if args.chats else allowed)
    try:
        bot.run(only_list_chats=args.chats)
    except KeyboardInterrupt:
        print("\nостановлено", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
