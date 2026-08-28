#!/usr/bin/env bash
# Проверяет, что мост в Telegram жив. Вызывается хуком SessionStart при
# открытии сессии Claude Code по этому проекту.
#
# Демон живёт не здесь, а на сервере, где хостится сайт: там он работает
# круглосуточно, и сообщения тестировщиков не теряются, пока никто не открывал
# проект. Хук поэтому ничего не запускает локально — он только смотрит, всё ли
# в порядке, и поднимает сервис, если тот лежит.
#
# Локальный запуск (`--local`) остаётся для отладки без сервера. Держать оба
# одновременно нельзя: Telegram отдаёт каждое обновление ровно одному клиенту
# и второму отвечает Conflict, а часть переписки теряется молча.
set -u

HOST="${MESHTRX_HOST:-root@195.133.1.248}"
UNIT="meshtrx-tgbot"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=5)

if [ "${1:-}" = "--local" ]; then
    ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
    STATE="${XDG_DATA_HOME:-$HOME/.local/share}/meshtrx"
    ENV_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/meshtrx/telegram.env"
    [ -r "$ENV_FILE" ] || { echo "нет $ENV_FILE — локальный демон не запущен"; exit 0; }
    mkdir -p "$STATE"
    PY="$ROOT/.venv-desktop/bin/python"
    [ -x "$PY" ] || PY="$(command -v python3)"
    setsid "$PY" "$ROOT/tools/tgbot/daemon.py" >>"$STATE/daemon.log" 2>&1 < /dev/null &
    disown 2>/dev/null || true
    echo "мост в Telegram: запущен локально (сервер при этом трогать нельзя)"
    exit 0
fi

state=$(timeout 8 ssh "${SSH_OPTS[@]}" "$HOST" "systemctl is-active $UNIT" 2>/dev/null)

case "$state" in
    active)
        echo "мост в Telegram: работает на сервере"
        ;;
    "")
        # Сервер недоступен — это не повод шуметь: возможно, нет сети.
        echo "мост в Telegram: сервер не отвечает, состояние неизвестно"
        ;;
    *)
        timeout 12 ssh "${SSH_OPTS[@]}" "$HOST" "systemctl restart $UNIT" >/dev/null 2>&1
        again=$(timeout 8 ssh "${SSH_OPTS[@]}" "$HOST" "systemctl is-active $UNIT" 2>/dev/null)
        if [ "$again" = "active" ]; then
            echo "мост в Telegram: сервис лежал ($state), поднят"
        else
            echo "мост в Telegram: сервис не поднимается ($again) — journalctl -u $UNIT на $HOST"
        fi
        ;;
esac
exit 0
