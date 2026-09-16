#include "crypto.h"
#include "debug.h"
#include <string.h>
#include <Preferences.h>
#include <mbedtls/aes.h>
#include <mbedtls/cipher.h>
#include <mbedtls/cmac.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>

static bool     haveKey = false;
static uint8_t  keyEnc[32];     // ключ шифра
static uint8_t  keyMac[16];     // ключ кода аутентичности
static char     fingerprint[5] = "----";

// Эпоха отделяет один сеанс работы от другого. Счётчик пакетов у нас
// восьмибитный и обходит круг за двадцать секунд речи; если бы номер пакета
// был единственным, что меняется, поток шифровался бы одной и той же гаммой
// по кругу — а это раскрывает содержимое без всякого ключа.
static uint16_t epoch = 0;
static uint8_t  lastSeq = 0;
static uint16_t epochReserve = 0;   // сколько оборотов счётчика ещё покрыто

// При каждой загрузке эпоха прыгает вперёд с запасом: писать её в память на
// каждом обороте счётчика — это запись раз в двадцать секунд, и память такого
// не переживёт. Запас тратится в оперативной памяти, а до памяти устройства
// доходит раз в двадцать минут непрерывного разговора.
#define EPOCH_STEP  64

static void saveEpoch(uint16_t value) {
  Preferences prefs;
  prefs.begin("crypto", false);
  prefs.putUShort("epoch", value);
  prefs.end();
}

static void deriveKeys(const uint8_t* key) {
  uint8_t buf[CRYPTO_KEY_LEN + 4];
  memcpy(buf, key, CRYPTO_KEY_LEN);

  memcpy(buf + CRYPTO_KEY_LEN, "enc", 3);
  mbedtls_sha256(buf, CRYPTO_KEY_LEN + 3, keyEnc, 0);

  memcpy(buf + CRYPTO_KEY_LEN, "mac", 3);
  uint8_t macFull[32];
  mbedtls_sha256(buf, CRYPTO_KEY_LEN + 3, macFull, 0);
  memcpy(keyMac, macFull, 16);

  // Отпечаток — от самого ключа, одинаковый на всех устройствах группы
  memcpy(buf + CRYPTO_KEY_LEN, "fp", 2);
  uint8_t fp[32];
  mbedtls_sha256(buf, CRYPTO_KEY_LEN + 2, fp, 0);
  static const char* hexd = "0123456789ABCDEF";
  fingerprint[0] = hexd[fp[0] >> 4];
  fingerprint[1] = hexd[fp[0] & 0xF];
  fingerprint[2] = hexd[fp[1] >> 4];
  fingerprint[3] = hexd[fp[1] & 0xF];
  fingerprint[4] = 0;
}

void cryptoInit() {
  Preferences prefs;
  prefs.begin("crypto", false);
  uint8_t key[CRYPTO_KEY_LEN];
  size_t got = prefs.getBytes("key", key, sizeof(key));
  uint16_t stored = prefs.getUShort("epoch", 0);
  prefs.end();

  epoch = stored + 1;
  epochReserve = EPOCH_STEP - 1;
  saveEpoch(epoch + EPOCH_STEP - 1);

  if (got == CRYPTO_KEY_LEN) {
    deriveKeys(key);
    haveKey = true;
    LOG_F("[Crypto] ключ канала загружен, отпечаток %s\n", fingerprint);
  } else {
    LOG_D("[Crypto] ключа нет — эфир открыт");
  }
}

bool cryptoHasKey() { return haveKey; }
const char* cryptoKeyFingerprint() { return haveKey ? fingerprint : "----"; }

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool cryptoSetKeyHex(const char* hex) {
  if (!hex || strlen(hex) != CRYPTO_KEY_LEN * 2) return false;
  uint8_t key[CRYPTO_KEY_LEN];
  for (int i = 0; i < CRYPTO_KEY_LEN; i++) {
    int hi = hexVal(hex[i * 2]), lo = hexVal(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    key[i] = (uint8_t)((hi << 4) | lo);
  }
  Preferences prefs;
  prefs.begin("crypto", false);
  prefs.putBytes("key", key, sizeof(key));
  prefs.end();
  deriveKeys(key);
  haveKey = true;
  LOG_F("[Crypto] ключ установлен, отпечаток %s\n", fingerprint);
  return true;
}

// Сколько раз прогоняется преобразование пароля. Замер на устройстве: 120 000
// проходов это 7,4 секунды — столько человек ждать не будет, а перебор на
// обычном компьютере всё равно идёт на порядки быстрее. Поэтому берём 50 000
// (около трёх секунд) и не делаем вид, что итерации спасают: от перебора
// защищает длина фразы, а не эти секунды.
#define CRYPTO_KDF_ROUNDS 50000

bool cryptoSetPassphrase(const char* phrase) {
  if (!phrase) return false;
  size_t n = strlen(phrase);
  // Считаем символы, а не байты: в кириллице байт вдвое больше букв, и
  // проверка «не короче восьми» пропускала фразу из пяти русских слогов.
  size_t chars = 0;
  for (size_t i = 0; i < n; i++)
    if ((phrase[i] & 0xC0) != 0x80) chars++;
  if (chars < CRYPTO_PASS_MIN) return false;

  // Соль постоянная и открытая: у нас нет места хранить её на каждом
  // устройстве, а задача соли здесь — развести наши ключи с чужими системами,
  // где может оказаться та же фраза, а не скрыть что-то от слушателя.
  static const char* salt = "MeshTRX channel key v1";
  uint8_t key[CRYPTO_KEY_LEN];
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md) return false;

  uint32_t t0 = millis();
  int rc = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
             (const unsigned char*)phrase, n,
             (const unsigned char*)salt, strlen(salt),
             CRYPTO_KDF_ROUNDS, CRYPTO_KEY_LEN, key);
  if (rc != 0) { LOG_F("[Crypto] не удалось вывести ключ: %d\n", rc); return false; }

  Preferences prefs;
  prefs.begin("crypto", false);
  prefs.putBytes("key", key, sizeof(key));
  prefs.end();
  deriveKeys(key);
  haveKey = true;
  LOG_F("[Crypto] ключ из кодового слова за %lu мс, отпечаток %s\n",
        (unsigned long)(millis() - t0), fingerprint);
  return true;
}

void cryptoClearKey() {
  Preferences prefs;
  prefs.begin("crypto", false);
  prefs.remove("key");
  prefs.end();
  haveKey = false;
  memset(keyEnc, 0, sizeof(keyEnc));
  memset(keyMac, 0, sizeof(keyMac));
  strcpy(fingerprint, "----");
  LOG_D("[Crypto] ключ удалён — эфир снова открыт");
}

// Блок счётчика: всё, что делает поток гаммы неповторимым. Ttl сюда не входит
// намеренно — его уменьшает ретранслятор, и пакет после него обязан остаться
// расшифровываемым.
static void makeNonce(uint8_t* nonce, uint8_t type, uint8_t channel,
                      const uint8_t* sender, uint8_t seq, uint16_t ep) {
  memset(nonce, 0, 16);
  nonce[0] = 0x01;                    // версия формата
  nonce[1] = type;
  nonce[2] = channel & PKT_CH_MASK;
  nonce[3] = sender[0];
  nonce[4] = sender[1];
  nonce[5] = seq;
  nonce[6] = (uint8_t)(ep >> 8);
  nonce[7] = (uint8_t)(ep & 0xFF);
}

static void ctrCrypt(uint8_t* data, size_t len, const uint8_t* nonce) {
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, keyEnc, 256);
  uint8_t counter[16], stream[16];
  memcpy(counter, nonce, 16);
  size_t off = 0;
  mbedtls_aes_crypt_ctr(&ctx, len, &off, counter, stream, data, data);
  mbedtls_aes_free(&ctx);
}

// Код аутентичности считается по счётчику и шифротексту: подменить нагрузку
// или выдать чужой пакет за свой без ключа не выйдет.
static bool computeTag(const uint8_t* nonce, const uint8_t* data, size_t len,
                       uint8_t* tagOut) {
  uint8_t full[16];
  const mbedtls_cipher_info_t* info =
      mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
  if (!info) return false;
  uint8_t buf[16 + 256];
  if (len > 256) return false;
  memcpy(buf, nonce, 16);
  memcpy(buf + 16, data, len);
  if (mbedtls_cipher_cmac(info, keyMac, 128, buf, 16 + len, full) != 0) return false;
  memcpy(tagOut, full, CRYPTO_TAG_LEN);
  return true;
}

int cryptoSeal(uint8_t type, uint8_t channel, const uint8_t* sender, uint8_t seq,
               uint8_t* buf, int len, int headerLen) {
  if (!haveKey || len <= headerLen) return len;

  // Счётчик пакетов пошёл на второй круг — сдвигаем эпоху
  if (seq < lastSeq) {
    epoch++;
    if (epochReserve) epochReserve--;
    else { saveEpoch(epoch + EPOCH_STEP); epochReserve = EPOCH_STEP; }
  }
  lastSeq = seq;

  uint8_t nonce[16];
  makeNonce(nonce, type, channel, sender, seq, epoch);

  uint8_t* payload = buf + headerLen;
  int payloadLen = len - headerLen;
  ctrCrypt(payload, payloadLen, nonce);

  uint8_t tag[CRYPTO_TAG_LEN];
  if (!computeTag(nonce, payload, payloadLen, tag)) return len;

  buf[len]     = (uint8_t)(epoch >> 8);
  buf[len + 1] = (uint8_t)(epoch & 0xFF);
  memcpy(buf + len + CRYPTO_EPOCH_LEN, tag, CRYPTO_TAG_LEN);
  buf[1] |= PKT_CH_ENCRYPTED;
  return len + CRYPTO_OVERHEAD;
}

int cryptoOpen(uint8_t type, uint8_t channel, const uint8_t* sender, uint8_t seq,
               uint8_t* buf, int len, int headerLen) {
  if (!(channel & PKT_CH_ENCRYPTED)) return 0;          // открытый пакет
  if (!haveKey) return -1;
  if (len < headerLen + CRYPTO_OVERHEAD) return -1;

  int bodyLen = len - CRYPTO_OVERHEAD;
  int payloadLen = bodyLen - headerLen;
  uint16_t ep = ((uint16_t)buf[bodyLen] << 8) | buf[bodyLen + 1];

  uint8_t nonce[16];
  makeNonce(nonce, type, channel, sender, seq, ep);

  uint8_t* payload = buf + headerLen;
  uint8_t tag[CRYPTO_TAG_LEN];
  if (!computeTag(nonce, payload, payloadLen, tag)) return -1;
  if (memcmp(tag, buf + bodyLen + CRYPTO_EPOCH_LEN, CRYPTO_TAG_LEN) != 0) return -1;

  ctrCrypt(payload, payloadLen, nonce);
  buf[1] &= PKT_CH_MASK;
  return bodyLen;
}
