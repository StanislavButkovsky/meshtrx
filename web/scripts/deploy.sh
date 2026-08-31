#!/usr/bin/env bash
#
# Выкладка сайта на VPS. До сих пор это делалось руками, и порядок шагов жил
# только в PROJECT_LOG.md — а забыть там легко ровно то, что дороже всего:
# снять архив прежней версии перед перезаписью.
#
# Сайт собирается дважды: meshtrx.ru по-русски, meshtrx.com по-английски.
# Разница не только в языке по умолчанию — она в самом HTML, поэтому одной
# сборкой обойтись нельзя: поисковик и превью ссылки JS не выполняют.
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
  trap 'rm -f "$PASSFILE"; rm -rf out-ru out-en' EXIT
  SSH="sshpass -f $PASSFILE ssh -o StrictHostKeyChecking=accept-new"
else
  trap 'rm -rf out-ru out-en' EXIT
  SSH="ssh -o StrictHostKeyChecking=accept-new"
fi

# Собирать надо при остановленном dev-сервере: они пишут в один .next и ломают
# друг друга — сборка падает с PageNotFoundError, а сервер потом отдаёт 500.
if pgrep -f "next dev" >/dev/null; then
  echo "Запущен dev-сервер. Остановите его: pkill -f 'next dev'" >&2
  exit 1
fi

rm -rf out-ru out-en
for loc in ru en; do
  echo "==> Сборка $loc"
  rm -rf .next out
  NEXT_PUBLIC_SITE_LOCALE=$loc npm run build >/dev/null
  [ -f out/index.html ] || { echo "Сборка $loc не дала out/index.html" >&2; exit 1; }
  # Проверка, что переменная действительно доехала до разметки: перепутанные
  # местами каталоги — ошибка, которую на глаз замечают не сразу.
  grep -q "<html lang=\"$loc\"" out/index.html || {
    echo "В сборке $loc разметка отдана не на том языке" >&2; exit 1; }
  mv out "out-$loc"
  echo "    out-$loc готов, $(du -sh "out-$loc" | cut -f1)"
done

echo "==> Что изменится на сервере"
for loc in ru en; do
  echo "  -- $MESHTRX_DEPLOY_PATH-$loc --"
  rsync -az --delete --dry-run --itemize-changes \
    -e "$SSH" "out-$loc/" "$TARGET:$MESHTRX_DEPLOY_PATH-$loc/" | sed 's/^/    /'
done

[ "$MODE" = dry ] && { echo "==> Только показ, ничего не менялось"; exit 0; }

if [ "$MODE" = ask ]; then
  read -rp "Выкладываем на $MESHTRX_DEPLOY_HOST? [y/N] " ans
  [ "$ans" = y ] || [ "$ans" = Y ] || { echo "Отменено"; exit 0; }
fi

STAMP=$(date +%Y%m%d-%H%M)
ARCHIVE="$MESHTRX_BACKUP_DIR/meshtrx-backup-$STAMP.tar.gz"
echo "==> Архив прежней версии: $ARCHIVE"
$SSH "$TARGET" "cd /var/www && tar czf '$ARCHIVE' \$(ls -d meshtrx meshtrx-ru meshtrx-en 2>/dev/null)"

for loc in ru en; do
  echo "==> Выгрузка $loc → $MESHTRX_DEPLOY_PATH-$loc"
  # Файлы на сервере принадлежат 1000:1000, а ходим мы root'ом: без --chown
  # новые файлы легли бы от root и каталог стал бы разношёрстным.
  rsync -az --delete --chown=1000:1000 \
    -e "$SSH" "out-$loc/" "$TARGET:$MESHTRX_DEPLOY_PATH-$loc/"
done

echo "==> Проверка"
fail=0
check() { # домен, ожидаемый язык
  for path in / /articles/ /docs/ /download/ /flash/ /about/ /sitemap.xml; do
    code=$(curl -s -o /dev/null -w '%{http_code}' -m 30 "$1$path" || echo 000)
    [ "$code" = 200 ] || { echo "    $code $1$path" >&2; fail=1; }
  done
  lang=$(curl -s -m 30 "$1/" | grep -o '<html lang="[a-z]*"' | head -1)
  [ "$lang" = "<html lang=\"$2\"" ] || { echo "    $1 отдаёт $lang, ждали lang=\"$2\"" >&2; fail=1; }
}
check https://meshtrx.ru ru
check https://meshtrx.com en
if [ "$fail" = 0 ]; then
  echo "    meshtrx.ru отдаёт русский, meshtrx.com английский, все страницы 200"
else
  echo "==> Что-то не так. Откат: tar xzf $ARCHIVE -C /var/www" >&2
  exit 1
fi
# public/llms.txt лежит в репозитории, но собирается заново перед каждой
# сборкой. После двух сборок подряд в дереве остаётся английский вариант, и
# git показывает изменения там, где никто ничего не правил. Возвращаем к тому,
# что отслеживается: русский — это же язык сборки по умолчанию.
NEXT_PUBLIC_SITE_LOCALE=ru node scripts/gen-llms.mjs >/dev/null

echo "==> Готово"
