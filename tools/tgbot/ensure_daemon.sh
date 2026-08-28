#!/usr/bin/env bash
# Поднимает демона Telegram, если он ещё не работает. Вызывается хуком
# SessionStart при открытии сессии Claude Code по этому проекту.
#
# Единственность держится на блокировке файла, а не на поиске по списку
# процессов: два демона начали бы отбирать обновления друг у друга, и часть
# переписки пропала бы молча. Блокировку берёт сам daemon.py — здесь мы лишь
# не мешаем ему это сделать.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STATE="${XDG_DATA_HOME:-$HOME/.local/share}/meshtrx"
LOG="$STATE/daemon.log"
LOCK="$STATE/daemon.lock"
ENV_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/meshtrx/telegram.env"

# Без токена запускать нечего: демон всё равно выйдет с ошибкой.
[ -r "$ENV_FILE" ] || { echo "мост в Telegram: нет $ENV_FILE — демон не запущен"; exit 0; }

mkdir -p "$STATE"

# Занятая блокировка означает, что демон уже работает.
if command -v flock >/dev/null && [ -e "$LOCK" ] && ! flock -n "$LOCK" true 2>/dev/null; then
    echo "мост в Telegram: демон уже работает"
    exit 0
fi

PY="$ROOT/.venv-desktop/bin/python"
[ -x "$PY" ] || PY="$(command -v python3)"
[ -n "$PY" ] || { echo "мост в Telegram: не нашёл python"; exit 0; }

# Журнал режем по мегабайту: он пишется годами и никем не читается целиком.
if [ -f "$LOG" ] && [ "$(stat -c %s "$LOG" 2>/dev/null || echo 0)" -gt 1048576 ]; then
    mv -f "$LOG" "$LOG.1"
fi

# setsid отвязывает демона от терминала сессии: закрытие Claude Code его не убьёт.
setsid "$PY" "$ROOT/tools/tgbot/daemon.py" >>"$LOG" 2>&1 < /dev/null &
disown 2>/dev/null || true

sleep 1
if kill -0 $! 2>/dev/null; then
    echo "мост в Telegram: демон запущен, журнал $LOG"
else
    echo "мост в Telegram: демон не поднялся, смотрите $LOG"
fi
exit 0
