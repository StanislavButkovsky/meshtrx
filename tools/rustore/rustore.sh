#!/usr/bin/env bash
# RuStore API (взят из obd-app, docs/rustore_api.md там): токен, статус версий, публикация новой версии.
#   tools/rustore/rustore.sh auth                  — проверить ключ (получить токен)
#   tools/rustore/rustore.sh status                — версии приложения и их статусы
#   tools/rustore/rustore.sh publish APK "Что нового"   — черновик → APK → на модерацию
#   tools/rustore/rustore.sh release VERSION_ID        — выпустить одобренную версию
#   tools/rustore/rustore.sh release-if-ready VERSION_ID — выпустить, если уже одобрена;
#     код 0 — выпущена, 3 — ещё на модерации, 4 — отклонена (для разовой проверки по cron)
# Ключ — ~/.rustore/meshtrx_api_key (строки «id …» и «key …», вне репозитория);
# путь можно задать в RUSTORE_KEY. Значения ключа не выводим.
# Публикация видна всем — только по команде владельца. Первую версию API не
# публикует: она загружается вручную в RuStore Консоли.
set -euo pipefail
API=https://public-api.rustore.ru/public
PACKAGE="${RUSTORE_PACKAGE:-com.meshtrx.app}"
KEYFILE="${RUSTORE_KEY:-$HOME/.rustore/meshtrx_api_key}"

field() { sed -n "s/^$1[[:space:]=:]*//p" "$KEYFILE" | head -1 | tr -d '[:space:]"'; }

token() {
  local id ts sig pem body
  id="$(field id)"
  [ -n "$id" ] || { echo "в $KEYFILE нет строки id" >&2; exit 1; }
  pem="$(mktemp)"; chmod 600 "$pem"
  trap 'rm -f "$pem"' RETURN
  { echo '-----BEGIN PRIVATE KEY-----'; field key | fold -w 64; echo; echo '-----END PRIVATE KEY-----'; } > "$pem"
  ts="$(date +%Y-%m-%dT%H:%M:%S.%3N%:z)"
  # Подпись: SHA512withRSA от keyId + timestamp, в Base64.
  sig="$(printf '%s%s' "$id" "$ts" | openssl dgst -sha512 -sign "$pem" | base64 -w0)"
  body="$(curl -sS -X POST "$API/auth/" -H 'Content-Type: application/json' \
    -d "{\"keyId\":\"$id\",\"timestamp\":\"$ts\",\"signature\":\"$sig\"}")"
  printf '%s' "$body" | python3 -c '
import json, sys
d = json.load(sys.stdin)
b = d.get("body") if isinstance(d, dict) else None
jwe = b.get("jwe") if isinstance(b, dict) else None
if not jwe:
    sys.exit("токен не выдан: %s" % json.dumps(d, ensure_ascii=False)[:300])
print(jwe)'
}

ok() { python3 -c '
import json, sys
d = json.load(sys.stdin)
if d.get("code") != "OK":
    sys.exit("RuStore: %s %s" % (d.get("code"), d.get("message")))
print(json.dumps(d.get("body"), ensure_ascii=False))'; }

case "${1:-}" in
  auth)
    token >/dev/null && echo "ключ работает: токен получен (живёт 900 с)" ;;
  status)
    T="$(token)"
    B="$(curl -sS "$API/v1/application/$PACKAGE/version?filterTestingType=ALL&size=20" \
      -H "Public-Token: $T" | ok)"
    printf '%s' "$B" | python3 -c '
import json, sys
b = json.load(sys.stdin) or {}
for v in b.get("content", []):
    print(v.get("versionId"), v.get("versionName"), v.get("versionCode"), v.get("versionStatus"))
if not b.get("content"): print("версий нет")' ;;
  publish)
    APK="${2:?путь к APK}"; NEWS="${3:?текст «Что нового»}"
    [ -f "$APK" ] || { echo "нет файла $APK" >&2; exit 1; }
    T="$(token)"
    NEWS_JSON="$(python3 -c 'import json,sys; print(json.dumps(sys.argv[1]))' "$NEWS")"
    VID="$(curl -sS -X POST "$API/v1/application/$PACKAGE/version" \
      -H "Public-Token: $T" -H 'Content-Type: application/json' \
      -d "{\"whatsNew\":$NEWS_JSON,\"publishType\":\"MANUAL\"}" | ok)"
    echo "черновик: $VID"
    curl -sS -X POST "$API/v1/application/$PACKAGE/version/$VID/apk?isMainApk=true&servicesType=Unknown" \
      -H "Public-Token: $T" -F "file=@$APK" | ok >/dev/null
    echo "APK загружен"
    curl -sS -X POST "$API/v1/application/$PACKAGE/version/$VID/commit" \
      -H "Public-Token: $T" | ok >/dev/null
    echo "отправлено на модерацию; после одобрения — опубликовать в Консоли (publishType MANUAL)" ;;
  release)
    VID="${2:?versionId}"; T="$(token)"
    curl -sS -X POST "$API/v1/application/$PACKAGE/version/$VID/publish" \
      -H "Public-Token: $T" | ok >/dev/null
    echo "версия $VID выпущена" ;;
  release-if-ready)
    VID="${2:?versionId}"; T="$(token)"
    ST="$(curl -sS "$API/v1/application/$PACKAGE/version?filterTestingType=ALL&size=20" \
      -H "Public-Token: $T" | ok | python3 -c '
import json, sys
vid = sys.argv[1]
for v in (json.load(sys.stdin) or {}).get("content", []):
    if str(v.get("versionId")) == vid: print(v.get("versionStatus"))' "$VID")"
    echo "версия $VID: ${ST:-не найдена}"
    case "$ST" in
      READY_FOR_PUBLICATION) "$0" release "$VID" ;;
      ACTIVE) exit 0 ;;
      REJECTED*|"") exit 4 ;;
      *) exit 3 ;;
    esac ;;
  *) sed -n 2,13p "$0"; exit 64 ;;
esac
