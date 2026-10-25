#!/usr/bin/env bash
# RuStore через сервер сайта: RuStore не отвечает на подключения из-за
# границы, а этот компьютер выходит в сеть через зарубежный VPN.
#   tools/rustore/rustore-via-server.sh setup                — один раз положить ключ на сервер
#   tools/rustore/rustore-via-server.sh auth | status
#   tools/rustore/rustore-via-server.sh publish APK "Что нового"
# Сервер берётся из ~/.config/meshtrx/deploy.env (как у web/scripts/deploy.sh).
# Ключ на сервере — /root/.rustore/meshtrx_api_key (каталог 700, файл 600);
# скрипт и APK копируются во временный каталог и удаляются после запуска.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CONF="${MESHTRX_DEPLOY_ENV:-$HOME/.config/meshtrx/deploy.env}"
[ -r "$CONF" ] || { echo "Нет файла с доступом: $CONF" >&2; exit 1; }
set -a; . "$CONF"; set +a
: "${MESHTRX_DEPLOY_HOST:?не задан в $CONF}"
: "${MESHTRX_DEPLOY_USER:=root}"
DEST="$MESHTRX_DEPLOY_USER@$MESHTRX_DEPLOY_HOST"
REMOTE_KEY='$HOME/.rustore/meshtrx_api_key'
LOCAL_KEY="${RUSTORE_KEY:-$HOME/.rustore/meshtrx_api_key}"

case "${1:-}" in
  setup)
    [ -r "$LOCAL_KEY" ] || { echo "нет ключа $LOCAL_KEY" >&2; exit 1; }
    ssh "$DEST" 'umask 077; mkdir -p ~/.rustore && cat > ~/.rustore/meshtrx_api_key' < "$LOCAL_KEY"
    echo "ключ на сервере: ~/.rustore/meshtrx_api_key"
    exec "$0" auth ;;
  auth|status|publish) ;;
  *) sed -n 2,10p "$0"; exit 64 ;;
esac

TMP="$(ssh "$DEST" 'mktemp -d /tmp/rustore.XXXXXX')"
trap 'ssh "$DEST" "rm -rf $TMP"' EXIT
scp -q "$HERE/rustore.sh" "$DEST:$TMP/rustore.sh"
if [ "$1" = publish ]; then
  APK="${2:?путь к APK}"; NEWS="${3:?текст «Что нового»}"
  [ -f "$APK" ] || { echo "нет файла $APK" >&2; exit 1; }
  scp -q "$APK" "$DEST:$TMP/app.apk"
  ssh "$DEST" "RUSTORE_KEY=$REMOTE_KEY bash $TMP/rustore.sh publish $TMP/app.apk $(printf '%q' "$NEWS")"
else
  ssh "$DEST" "RUSTORE_KEY=$REMOTE_KEY bash $TMP/rustore.sh $1"
fi
