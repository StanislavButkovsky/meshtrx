#!/usr/bin/env python3
"""Выбирает адрес api.telegram.org, до которого с этой машины есть связь.

Зачем это нужно. На сервере в России имя api.telegram.org резолвится в
IPv6-адрес, до которого связи нет, а из известных IPv4-адресов Telegram
отвечает не каждый: часть отфильтрована по пути. Демон при этом не падает —
он молча получает ошибки сети и не отвечает в группе, что выглядит как
«бот сломался», а не как «провайдер режет».

Поэтому перед запуском демона мы находим живой адрес сами и закрепляем его в
/etc/hosts. TLS при этом остаётся честным: соединение идёт по имени, проверка
сертификата работает как обычно, подменяется только разрешение имени в адрес.

Запускается из systemd как ExecStartPre — то есть при каждом старте и при
каждом перезапуске после сбоя. Если завтра сегодняшний адрес перестанет
отвечать, демон упрётся в ошибки сети, выйдет, systemd поднимет его снова, и
маршрут переберётся заново без участия человека.
"""

from __future__ import annotations

import re
import socket
import ssl
import sys

HOSTS = "/etc/hosts"
NAME = "api.telegram.org"
MARK_BEGIN = "# --- meshtrx: маршрут до Telegram (подбирается автоматически)"
MARK_END = "# --- meshtrx: конец"

# Адреса центров Telegram. Список намеренно прибит гвоздями: системный резолв
# на этой машине отдаёт нерабочий вариант, ради которого всё и затевалось.
CANDIDATES = [
    "149.154.167.220",
    "149.154.167.51",
    "149.154.175.50",
    "149.154.171.5",
    "149.154.175.100",
    "91.108.56.130",
    "149.154.166.110",
]
TIMEOUT = 4.0


def alive(ip: str, attempts: int = 2) -> bool:
    """Проверяем не пинг, а то, что нужно на самом деле: устанавливается ли
    TLS-сессия с правильным именем. Открытый порт ещё ничего не значит —
    фильтрация обычно рвёт соединение уже после приветствия.

    Две попытки, а не одна: при первом же прогоне рабочий адрес не отозвался,
    а через минуту ответил за 0,2 с. Путь сюда нестабилен, и отбрасывать
    годный адрес по одной неудаче — значит остаться без связи на ровном месте.
    """
    return any(_probe(ip) for _ in range(attempts))


def _probe(ip: str) -> bool:
    ctx = ssl.create_default_context()
    try:
        with socket.create_connection((ip, 443), timeout=TIMEOUT) as raw:
            with ctx.wrap_socket(raw, server_hostname=NAME) as tls:
                tls.settimeout(TIMEOUT)
                tls.send(b"HEAD / HTTP/1.1\r\nHost: " + NAME.encode() + b"\r\n"
                         b"Connection: close\r\n\r\n")
                return bool(tls.recv(16))
    except OSError:
        return False


def write_hosts(ip: str) -> None:
    text = open(HOSTS, encoding="utf-8").read()
    block = f"{MARK_BEGIN}\n{ip}\t{NAME}\n{MARK_END}\n"
    pattern = re.compile(re.escape(MARK_BEGIN) + r".*?" + re.escape(MARK_END) + r"\n",
                         re.S)
    text = pattern.sub("", text).rstrip("\n") + "\n" + block
    open(HOSTS, "w", encoding="utf-8").write(text)


def main() -> int:
    for ip in CANDIDATES:
        if alive(ip):
            try:
                write_hosts(ip)
            except PermissionError:
                print(f"нет прав на {HOSTS}; рабочий адрес: {ip}", file=sys.stderr)
                return 1
            print(f"маршрут до {NAME}: {ip}")
            return 0
    # Не находим ни одного — выходим с ошибкой, чтобы это было видно в
    # состоянии сервиса, а не только в журнале.
    print(f"ни один адрес {NAME} не отвечает — Telegram недоступен с этой машины",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
