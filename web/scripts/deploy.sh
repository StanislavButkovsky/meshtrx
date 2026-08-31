#!/usr/bin/env bash
#
# Выкладка сайта на VPS. До сих пор это делалось руками, и порядок шагов жил
# только в PROJECT_LOG.md — а забыть там легко ровно то, что дороже всего:
# снять архив прежней версии перед перезаписью.
#
# Доступ лежит в ~/.config/meshtrx/deploy.env и в репозиторий не попадает.
#
#   ./scripts/deploy.sh            — показать, что изменится, и спросить
#   ./scripts/deploy.sh --yes      — выложить без вопроса
#   ./scripts/deploy.sh --dry-run  — только показать, ничего не трогая

set -euo pipefail

CONF="${MESHTRX_DEPLOY_ENV:-$HOME/.config/meshtrx/deploy.env}"
[ -r "$CONF" ] || { echo "Нет файла с доступом: $CONF" >&2; exit 1; }
# shellcheck disable=SC1090
set -a; . "$CONF"; set +a

: "${MESHTRX_DEPLOY_HOST:?не задан в $CONF}"
: "${MESHTRX_DEPLOY_USER:=root}"
: "${MESHTRX_DEPLOY_PATH:=/var/www/meshtrx}"
: "${MESHTRX_BACKUP_DIR:=/root}"

MODE=ask
case "${1:-}" in
  --yes|-y) MODE=go ;;
  --dry-run|-n) MODE=dry ;;
  '') ;;
  *) echo "Неизвестный ключ: $1" >&2; exit 1 ;;
esac

cd "$(dirname "$0")/.."
TARGET="$MESHTRX_DEPLOY_USER@$MESHTRX_DEPLOY_HOST"

# Пароль — временная мера, поэтому sshpass подключается, только если он задан.
# С ключом строка ниже остаётся пустой, и ssh работает как обычно.
if [ -n "${MESHTRX_DEPLOY_PASS:-}" ]; then
  command -v sshpass >/dev/null || { echo "Нужен sshpass либо ключ вместо пароля" >&2; exit 1; }
  PASSFILE=$(mktemp); chmod 600 "$PASSFILE"
  printf '%s' "$MESHTRX_DEPLOY_PASS" > "$PASSFILE"
  trap 'rm -f "$PASSFILE"' EXIT
  SSH="sshpass -f $PASSFILE ssh -o StrictHostKeyChecking=accept-new"
else
  SSH="ssh -o StrictHostKeyChecking=accept-new"
fi

echo "==> Сборка"
# Собирать надо при остановленном dev-сервере: они пишут в один .next и ломают
# друг друга — сборка падает с PageNotFoundError, а сервер потом отдаёт 500.
if pgrep -f "next dev|next-server" >/dev/null; then
  echo "    Запущен dev-сервер. Остановите его: pkill -f 'next dev'" >&2
  exit 1
fi
rm -rf .next out
npm run build >/dev/null
[ -f out/index.html ] || { echo "Сборка не дала out/index.html" >&2; exit 1; }
echo "    out/ готов, $(du -sh out | cut -f1)"

echo "==> Что изменится на сервере"
rsync -az --delete --dry-run --itemize-changes \
  -e "$SSH" out/ "$TARGET:$MESHTRX_DEPLOY_PATH/" | sed 's/^/    /'

[ "$MODE" = dry ] && { echo "==> Только показ, ничего не менялось"; exit 0; }

if [ "$MODE" = ask ]; then
  read -rp "Выкладываем на $MESHTRX_DEPLOY_HOST? [y/N] " ans
  [ "$ans" = y ] || [ "$ans" = Y ] || { echo "Отменено"; exit 0; }
fi

STAMP=$(date +%Y%m%d-%H%M)
ARCHIVE="$MESHTRX_BACKUP_DIR/meshtrx-backup-$STAMP.tar.gz"
echo "==> Архив прежней версии: $ARCHIVE"
$SSH "$TARGET" "tar czf '$ARCHIVE' -C '$(dirname "$MESHTRX_DEPLOY_PATH")' '$(basename "$MESHTRX_DEPLOY_PATH")'"

echo "==> Выгрузка"
# Файлы на сервере принадлежат 1000:1000, а ходим мы root'ом: без --chown
# новые файлы легли бы от root и каталог стал бы разношёрстным.
rsync -az --delete --chown=1000:1000 -e "$SSH" out/ "$TARGET:$MESHTRX_DEPLOY_PATH/"

echo "==> Проверка"
fail=0
for host in https://meshtrx.com https://meshtrx.ru; do
  for path in / /articles/ /docs/ /download/ /flash/ /about/ /sitemap.xml; do
    code=$(curl -s -o /dev/null -w '%{http_code}' -m 30 "$host$path" || echo 000)
    [ "$code" = 200 ] || { echo "    $code $host$path" >&2; fail=1; }
  done
done
if [ "$fail" = 0 ]; then
  echo "    оба домена отвечают 200"
else
  echo "==> Что-то отдаёт не 200. Откат: tar xzf $ARCHIVE -C $(dirname "$MESHTRX_DEPLOY_PATH")" >&2
  exit 1
fi
echo "==> Готово"
