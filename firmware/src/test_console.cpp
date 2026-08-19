#ifdef TEST_CONSOLE

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "test_console.h"
#include "packet.h"
#include "lora_radio.h"
#include "ble_service.h"
#include "beacon.h"
#include "battery.h"
#include "utils.h"
#include <esp_random.h>
#include <freertos/FreeRTOS.h>

// Событие печатаем одной операцией записи и с принудительным сбросом буфера.
// На V4 консоль идёт через нативный USB CDC, и серия отдельных printf рвалась
// посередине: строки склеивались и обрезались, а стенд читал это как потерю
// пакета, которой на самом деле не было.
static void evt(const char* fmt, ...) {
  char buf[320];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if (n > (int)sizeof(buf) - 1) n = sizeof(buf) - 1;
  Serial.write((const uint8_t*)buf, n);
  Serial.flush();
}

#include <freertos/task.h>

// ================================================================
// Состояние
// ================================================================
static char   lineBuf[192];
static size_t lineLen = 0;
static bool   testMode = false;

// === Статистика приёма ===
struct RxTypeStat {
  uint8_t  type;
  uint32_t count;
};
static RxTypeStat rxStats[16];
static uint8_t    rxTypeCount = 0;

static uint32_t rxTotal = 0;
static uint32_t rxAudioLost = 0;     // пропуски по seq аудио
static uint32_t rxAudioDup = 0;
static int32_t  rxLastAudioSeq = -1;
static int32_t  rssiSum = 0;
static int16_t  rssiMin = 0;
static int16_t  rssiMax = -200;
static int32_t  snrSum = 0;
static uint32_t rssiSamples = 0;
static uint32_t rxFirstMs = 0;
static uint32_t rxLastMs = 0;

bool testConsoleIsActive() { return testMode; }

// === Эмуляция потерь канала (LOSS) ===
static uint8_t  lossPercent = 0;
static bool     lossAllTypes = false;   // false → теряем только чанки файла
static uint32_t rxDropped = 0;

bool testConsoleShouldDrop(uint8_t pktType) {
  if (lossPercent == 0) return false;
  if (!lossAllTypes && pktType != PKT_TYPE_FILE_CHUNK) return false;
  if ((esp_random() % 100) >= lossPercent) return false;
  rxDropped++;
  Serial.printf("EVT LORA_DROP type=%02X total_dropped=%lu\n",
    pktType, (unsigned long)rxDropped);
  return true;
}

// === Фоновая нагрузка (LOAD) ===
enum LoadProfile { LOAD_NONE, LOAD_TEXT, LOAD_AUDIO, LOAD_BEACON, LOAD_MIXED };
static volatile LoadProfile loadProfile = LOAD_NONE;
static volatile uint16_t    loadIntervalMs = 1000;
static volatile uint16_t    loadDest = 0;
static volatile uint32_t    loadSentText = 0, loadSentAudio = 0, loadSentBeacon = 0;
static TaskHandle_t         loadTaskHandle = nullptr;

static void loadTaskFunc(void* param) {
  uint32_t counter = 0;
  uint32_t lastBeacon = 0;
  while (true) {
    LoadProfile prof = loadProfile;
    if (prof == LOAD_NONE) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    uint32_t now = millis();
    char buf[48];

    if (prof == LOAD_TEXT || prof == LOAD_MIXED) {
      snprintf(buf, sizeof(buf), "LOAD-%lu", (unsigned long)counter);
      if (testHookSendText(loadDest, buf)) loadSentText++;
    }
    if (prof == LOAD_AUDIO || prof == LOAD_MIXED) {
      if (testHookSendAudioOne(0)) loadSentAudio++;
    }
    if ((prof == LOAD_BEACON || prof == LOAD_MIXED) && now - lastBeacon > 10000) {
      lastBeacon = now;
      if (beaconSendNow()) loadSentBeacon++;
    }
    counter++;
    vTaskDelay(pdMS_TO_TICKS(loadIntervalMs));
  }
}

static void loadStats() {
  Serial.printf("EVT LOAD_STATS profile=%d text=%lu audio=%lu beacon=%lu interval=%u\n",
    (int)loadProfile, (unsigned long)loadSentText, (unsigned long)loadSentAudio,
    (unsigned long)loadSentBeacon, loadIntervalMs);
}

static void statAdd(uint8_t type) {
  for (uint8_t i = 0; i < rxTypeCount; i++) {
    if (rxStats[i].type == type) { rxStats[i].count++; return; }
  }
  if (rxTypeCount < 16) {
    rxStats[rxTypeCount].type = type;
    rxStats[rxTypeCount].count = 1;
    rxTypeCount++;
  }
}

static void statReset() {
  rxDropped = 0;
  rxTypeCount = 0;
  rxTotal = rxAudioLost = rxAudioDup = 0;
  rxLastAudioSeq = -1;
  rssiSum = snrSum = 0;
  rssiSamples = 0;
  rssiMin = 0; rssiMax = -200;
  rxFirstMs = rxLastMs = 0;
}

// ================================================================
// Хуки из прошивки
// ================================================================
void testConsoleOnLoRaRx(const uint8_t* data, int len, int16_t rssi, int8_t snr) {
  if (len < 1) return;
  uint8_t type = data[0];
  rxTotal++;
  statAdd(type);

  uint32_t now = millis();
  if (rxFirstMs == 0) rxFirstMs = now;
  rxLastMs = now;

  rssiSum += rssi; snrSum += snr; rssiSamples++;
  if (rssi < rssiMin || rssiMin == 0) rssiMin = rssi;
  if (rssi > rssiMax) rssiMax = rssi;

  // Потери по seq — только для аудио (единственный поток с плотной нумерацией)
  int seq = -1;
  if (type == PKT_TYPE_AUDIO && len >= (int)sizeof(LoRaAudioPacket)) {
    const LoRaAudioPacket* pkt = (const LoRaAudioPacket*)data;
    seq = pkt->seq;
    if (rxLastAudioSeq >= 0) {
      int diff = (seq - rxLastAudioSeq) & 0xFF;
      if (diff == 0)      rxAudioDup++;
      else if (diff > 1)  rxAudioLost += (diff - 1);
    }
    rxLastAudioSeq = seq;
  }

  evt("EVT LORA_RX type=%02X len=%d rssi=%d snr=%d seq=%d t=%lu\n",
    type, len, rssi, snr, seq, (unsigned long)now);

  // Текст выводим отдельно — стенд сверяет целостность (в т.ч. UTF-8)
  if (type == PKT_TYPE_TEXT && len >= 9) {
    const LoRaTextPacket* t = (const LoRaTextPacket*)data;
    size_t tlen = strnlen((const char*)t->text, 85);
    uint16_t crc = crc16_ccitt(t->text, tlen);
    char hex[181];
    size_t hn = 0;
    for (size_t i = 0; i < tlen && i < 90; i++)
      hn += snprintf(hex + hn, sizeof(hex) - hn, "%02X", t->text[i]);
    hex[hn] = 0;
    evt("EVT TEXT_RX seq=%d dest=%04X len=%u crc=%04X hex=%s\n",
      t->seq, (unsigned)(t->dest[0] | (t->dest[1] << 8)),
      (unsigned)tlen, crc, hex);
  }
}

void testConsoleOnFileRxDone(const char* name, uint32_t size, uint16_t chunksTotal,
                             uint16_t chunksUnique, const uint8_t* buf) {
  // Проверка содержимого: тестовый файл заполнен детерминированным паттерном
  uint32_t badBytes = 0;
  uint16_t crc = 0;
  if (buf && size > 0) {
    crc = crc16_ccitt(buf, size);
    for (uint32_t i = 0; i < size; i++) {
      if (buf[i] != testFilePatternByte(i)) badBytes++;
    }
  }
  Serial.printf("EVT FILE_RX name=%s size=%lu chunks=%u/%u crc=%04X bad=%lu pattern_ok=%d\n",
    name ? name : "?", (unsigned long)size, chunksUnique, chunksTotal,
    crc, (unsigned long)badBytes, badBytes == 0 ? 1 : 0);
}

void testConsoleOnFileTxDone(bool delivered, uint32_t ms, uint8_t nackRounds) {
  Serial.printf("EVT FILE_TX result=%s ms=%lu nack_rounds=%u\n",
    delivered ? "DELIVERED" : "FAILED", (unsigned long)ms, nackRounds);
}

// ================================================================
// Разбор команд
// ================================================================
static char* nextTok(char** p) {
  while (**p == ' ') (*p)++;
  if (**p == 0) return nullptr;
  char* start = *p;
  while (**p && **p != ' ') (*p)++;
  if (**p) { **p = 0; (*p)++; }
  return start;
}

static void upcase(char* s) {
  for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32;
}

static uint16_t parseDest(const char* s) {
  if (!s) return 0;
  if (strcasecmp(s, "BCAST") == 0 || strcasecmp(s, "ALL") == 0) return 0;
  return (uint16_t)strtoul(s, nullptr, 16);
}

static uint8_t parseFileType(const char* s) {
  if (!s) return FILE_TYPE_BINARY;
  if (strcasecmp(s, "photo") == 0)    return FILE_TYPE_PHOTO;
  if (strcasecmp(s, "text") == 0)     return FILE_TYPE_TEXT;
  if (strcasecmp(s, "bin") == 0)      return FILE_TYPE_BINARY;
  if (strcasecmp(s, "voice") == 0)    return FILE_TYPE_VOICE;
  if (strcasecmp(s, "pttvoice") == 0) return FILE_TYPE_PTT_VOICE;
  return FILE_TYPE_BINARY;
}

static void cmdRxStats() {
  uint32_t span = (rxLastMs > rxFirstMs) ? (rxLastMs - rxFirstMs) : 0;
  Serial.printf("EVT RX_STATS total=%lu lost=%lu dup=%lu dropped=%lu loss_pct=%u "
                "rssi_min=%d rssi_max=%d rssi_avg=%d snr_avg=%d span_ms=%lu\n",
    (unsigned long)rxTotal, (unsigned long)rxAudioLost, (unsigned long)rxAudioDup,
    (unsigned long)rxDropped, lossPercent,
    rssiSamples ? rssiMin : 0, rssiSamples ? rssiMax : 0,
    rssiSamples ? (int)(rssiSum / (int32_t)rssiSamples) : 0,
    rssiSamples ? (int)(snrSum / (int32_t)rssiSamples) : 0,
    (unsigned long)span);
  for (uint8_t i = 0; i < rxTypeCount; i++) {
    Serial.printf("EVT RX_TYPE type=%02X count=%lu\n",
      rxStats[i].type, (unsigned long)rxStats[i].count);
  }
  Serial.println("EVT RX_STATS_END");
}

static void setTestMode(bool on) {
  testMode = on;
  if (!on) {
    loadProfile = LOAD_NONE;   // не оставлять фоновый трафик после тестов
    lossPercent = 0;
  }
  if (on) {
    loraSetTxPower(TEST_TX_POWER_DBM);   // минимум — устройства лежат рядом
    loraSetDutyCycle(false);
    loraSetPowerMode(LORA_POWER_CONTINUOUS_RX);
    statReset();
  }
  Serial.printf("EVT TESTMODE state=%s tx_power=%d\n", on ? "ON" : "OFF", loraGetTxPower());
}

static void handleLine(char* line) {
  char* p = line;
  char* cmd = nextTok(&p);
  if (!cmd) return;
  upcase(cmd);

  // --- Базовые ---
  if (strcmp(cmd, "PING") == 0) {
    Serial.printf("EVT PONG name=%s cs=%s uptime=%lu testmode=%d\n",
      bleGetDeviceName().c_str(), beaconGetCallSign(),
      (unsigned long)(millis() / 1000), testMode ? 1 : 0);
    return;
  }

  if (strcmp(cmd, "INFO") == 0) { testHookInfo(); return; }

  if (strcmp(cmd, "TESTMODE") == 0) {
    char* arg = nextTok(&p);
    setTestMode(arg && strcasecmp(arg, "OFF") != 0);
    return;
  }

  if (strcmp(cmd, "REBOOT") == 0) {
    Serial.println("EVT REBOOTING");
    Serial.flush();
    ESP.restart();
    return;
  }

  // --- Радио ---
  if (strcmp(cmd, "CH") == 0) {
    char* arg = nextTok(&p);
    if (!arg) { Serial.println("EVT ERR cmd=CH reason=no_arg"); return; }
    uint8_t ch = (uint8_t)atoi(arg);
    bool ok = testHookSetChannel(ch);
    Serial.printf("EVT CH ch=%d freq=%.3f ok=%d\n", ch, loraGetFrequency(ch), ok ? 1 : 0);
    return;
  }

  if (strcmp(cmd, "PWR") == 0) {
    char* arg = nextTok(&p);
    if (!arg) { Serial.println("EVT ERR cmd=PWR reason=no_arg"); return; }
    loraSetTxPower((int8_t)atoi(arg));
    Serial.printf("EVT PWR tx_power=%d\n", loraGetTxPower());
    return;
  }

  if (strcmp(cmd, "LORA") == 0) {
    char* arg = nextTok(&p);
    if (!arg) { Serial.println("EVT ERR cmd=LORA reason=no_arg"); return; }
    if (strcasecmp(arg, "rx") == 0)           loraSetPowerMode(LORA_POWER_CONTINUOUS_RX);
    else if (strcasecmp(arg, "duty") == 0)    loraSetPowerMode(LORA_POWER_DUTY_CYCLE_RX);
    else if (strcasecmp(arg, "standby") == 0) loraSetPowerMode(LORA_POWER_SLEEP);
    Serial.printf("EVT LORA_MODE mode=%d\n", (int)loraGetPowerMode());
    return;
  }

  // --- Эмуляция потерь канала ---
  if (strcmp(cmd, "LOSS") == 0) {
    char* pct = nextTok(&p);
    char* mode = nextTok(&p);
    lossPercent = pct ? (uint8_t)atoi(pct) : 0;
    if (lossPercent > 100) lossPercent = 100;
    lossAllTypes = mode && strcasecmp(mode, "ALL") == 0;
    rxDropped = 0;
    Serial.printf("EVT LOSS pct=%u scope=%s\n",
      lossPercent, lossAllTypes ? "ALL" : "CHUNKS");
    return;
  }

  // --- Фоновая нагрузка ---
  if (strcmp(cmd, "LOAD") == 0) {
    char* what = nextTok(&p);
    if (!what) { loadStats(); return; }
    upcase(what);

    if (strcmp(what, "STOP") == 0) {
      loadProfile = LOAD_NONE;
      Serial.println("EVT LOAD_STOPPED");
      return;
    }
    if (strcmp(what, "STATS") == 0) { loadStats(); return; }

    if (strcmp(what, "START") == 0) {
      char* prof = nextTok(&p);
      char* iv   = nextTok(&p);
      char* dst  = nextTok(&p);
      LoadProfile np = LOAD_MIXED;
      if (prof) {
        if      (strcasecmp(prof, "text")   == 0) np = LOAD_TEXT;
        else if (strcasecmp(prof, "audio")  == 0) np = LOAD_AUDIO;
        else if (strcasecmp(prof, "beacon") == 0) np = LOAD_BEACON;
        else if (strcasecmp(prof, "mixed")  == 0) np = LOAD_MIXED;
      }
      loadIntervalMs = iv ? (uint16_t)atoi(iv) : 1000;
      if (loadIntervalMs < 50) loadIntervalMs = 50;
      loadDest = parseDest(dst);
      loadSentText = loadSentAudio = loadSentBeacon = 0;
      if (!loadTaskHandle) {
        xTaskCreatePinnedToCore(loadTaskFunc, "load", 4096, nullptr, 2, &loadTaskHandle, 1);
      }
      loadProfile = np;
      Serial.printf("EVT LOAD_STARTED profile=%d interval=%u dest=%04X\n",
        (int)np, loadIntervalMs, loadDest);
      return;
    }
    Serial.printf("EVT ERR cmd=LOAD reason=unknown arg=%s\n", what);
    return;
  }

  // --- Управление внешним усилителем V4 вручную ---
  // Уровни GC1109 подобраны по документации, но проверить их можно только
  // замером: команда меняет пины на лету, а RX STATS показывает результат.
  if (strcmp(cmd, "FEM") == 0) {
#ifdef BOARD_V4
    char* a1 = nextTok(&p);
    char* a2 = nextTok(&p);
    char* a3 = nextTok(&p);
    if (a1 && a2 && a3) {
      digitalWrite(PA_FEM_POWER, atoi(a1) ? HIGH : LOW);
      digitalWrite(PA_FEM_EN,    atoi(a2) ? HIGH : LOW);
      digitalWrite(PA_FEM_CTX,   atoi(a3) ? HIGH : LOW);
    }
    evt("EVT FEM power=%d en=%d ctx=%d\n",
      digitalRead(PA_FEM_POWER), digitalRead(PA_FEM_EN), digitalRead(PA_FEM_CTX));
#else
    evt("EVT FEM board=v3 note=no_fem\n");
#endif
    return;
  }

  // --- Duty cycle RX: включение вручную, чтобы проверить режим отдельно ---
  if (strcmp(cmd, "DUTY") == 0) {
    char* arg = nextTok(&p);
    if (arg && strcasecmp(arg, "ON") == 0) {
      loraSetDutyCycle(true);
      loraSetPowerMode(LORA_POWER_DUTY_CYCLE_RX);
    } else if (arg && strcasecmp(arg, "OFF") == 0) {
      loraSetDutyCycle(false);
      loraSetPowerMode(LORA_POWER_CONTINUOUS_RX);
    }
    Serial.printf("EVT DUTY enabled=%d mode=%d\n",
      loraIsDutyCycleEnabled() ? 1 : 0, (int)loraGetPowerMode());
    return;
  }

  // --- Состояние радиочипа ---
  if (strcmp(cmd, "RADIO") == 0) {
    char* sub = nextTok(&p);
    if (sub && strcasecmp(sub, "TIME") == 0) {
      char* arg = nextTok(&p);
      if (arg && strcasecmp(arg, "RESET") == 0) {
        loraResetRadioTime();
        Serial.println("EVT RADIO_TIME_RESET");
        return;
      }
      uint32_t acc[RADIO_ACC_COUNT], window = 0, txn = 0;
      loraGetRadioTime(acc, &window, &txn);
      Serial.printf("EVT RADIO_TIME window=%lu standby=%lu rx=%lu tx=%lu "
                    "duty=%lu tx_count=%lu pwr=%d\n",
        (unsigned long)window, (unsigned long)acc[RADIO_ACC_STANDBY],
        (unsigned long)acc[RADIO_ACC_RX], (unsigned long)acc[RADIO_ACC_TX],
        (unsigned long)acc[RADIO_ACC_DUTY], (unsigned long)txn,
        (int)loraGetTxPower());
      return;
    }
    Serial.printf("EVT RADIO_STAT irq=%04X rx_armed=%d tx=%d mode=%d dio1=%d rxflag=%d t=%lu\n",
      loraGetIrqStatus(), loraIsRxArmed() ? 1 : 0, loraIsTxInProgress() ? 1 : 0,
      (int)loraGetPowerMode(), digitalRead(LORA_DIO1), loraRxFlag ? 1 : 0,
      (unsigned long)millis());
    return;
  }

  // --- Приём ---
  if (strcmp(cmd, "RX") == 0) {
    char* arg = nextTok(&p);
    if (arg && strcasecmp(arg, "RESET") == 0) {
      statReset();
      Serial.println("EVT RX_RESET");
    } else {
      cmdRxStats();
    }
    return;
  }

  // --- Передача ---
  if (strcmp(cmd, "TX") == 0) {
    char* what = nextTok(&p);
    if (!what) { Serial.println("EVT ERR cmd=TX reason=no_arg"); return; }
    upcase(what);

    if (strcmp(what, "TEXT") == 0) {
      char* dest = nextTok(&p);
      const char* text = p;  // остаток строки
      if (!dest || !text || !*text) { Serial.println("EVT ERR cmd=TX_TEXT reason=usage"); return; }
      bool ok = testHookSendText(parseDest(dest), text);
      Serial.printf("EVT TX_TEXT dest=%s ok=%d len=%d\n", dest, ok ? 1 : 0, (int)strlen(text));
      return;
    }

    if (strcmp(what, "AUDIO") == 0) {
      char* cntS = nextTok(&p);
      char* gapS = nextTok(&p);
      uint16_t cnt = cntS ? (uint16_t)atoi(cntS) : 10;
      uint16_t gap = gapS ? (uint16_t)atoi(gapS) : 80;
      Serial.printf("EVT TX_AUDIO_START count=%u gap=%u\n", cnt, gap);
      bool ok = testHookSendAudio(cnt, gap);
      Serial.printf("EVT TX_AUDIO_DONE ok=%d\n", ok ? 1 : 0);
      return;
    }

    if (strcmp(what, "PTT") == 0) {
      char* arg = nextTok(&p);
      bool on = arg && strcasecmp(arg, "OFF") != 0 && strcasecmp(arg, "END") != 0;
      testHookPtt(on);
      Serial.printf("EVT TX_PTT state=%s\n", on ? "ON" : "OFF");
      return;
    }

    if (strcmp(what, "FILE") == 0) {
      char* typeS = nextTok(&p);
      char* sizeS = nextTok(&p);
      char* destS = nextTok(&p);
      uint8_t  ft   = parseFileType(typeS);
      uint32_t size = sizeS ? (uint32_t)strtoul(sizeS, nullptr, 10) : 2048;
      uint16_t dest = parseDest(destS);
      bool ok = testHookSendFile(ft, size, dest);
      Serial.printf("EVT TX_FILE type=%02X size=%lu dest=%04X accepted=%d\n",
        ft, (unsigned long)size, dest, ok ? 1 : 0);
      return;
    }

    if (strcmp(what, "RAW") == 0) {
      // TX RAW <hex> — произвольные байты в эфир (фаззинг приёмной стороны)
      char* hex = nextTok(&p);
      if (!hex) { Serial.println("EVT ERR cmd=TX_RAW reason=no_arg"); return; }
      size_t hexLen = strlen(hex);
      if (hexLen < 2 || (hexLen & 1)) {
        Serial.println("EVT ERR cmd=TX_RAW reason=bad_hex");
        return;
      }
      size_t n = hexLen / 2;
      if (n > 222) n = 222;
      static uint8_t rawBuf[222];
      for (size_t i = 0; i < n; i++) {
        char b[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        rawBuf[i] = (uint8_t)strtoul(b, nullptr, 16);
      }
      bool ok = loraSend(rawBuf, n);
      loraStartReceive();
      Serial.printf("EVT TX_RAW len=%u type=%02X ok=%d\n",
        (unsigned)n, rawBuf[0], ok ? 1 : 0);
      return;
    }

    if (strcmp(what, "BEACON") == 0) {
      bool ok = beaconSendNow();
      loraStartReceive();
      Serial.printf("EVT TX_BEACON ok=%d\n", ok ? 1 : 0);
      return;
    }

    if (strcmp(what, "CALL") == 0) {
      char* kind = nextTok(&p);
      char* target = nextTok(&p);
      uint8_t t4[4] = {0};
      if (target && strlen(target) >= 8) {
        for (int i = 0; i < 4; i++) {
          char b[3] = { target[i*2], target[i*2+1], 0 };
          t4[i] = (uint8_t)strtoul(b, nullptr, 16);
        }
      }
      bool ok = testHookCall(kind ? kind : "all", t4);
      Serial.printf("EVT TX_CALL kind=%s ok=%d\n", kind ? kind : "all", ok ? 1 : 0);
      return;
    }

    Serial.printf("EVT ERR cmd=TX reason=unknown_subcmd arg=%s\n", what);
    return;
  }

  // --- Ответ на входящий вызов ---
  if (strcmp(cmd, "CALL") == 0) {
    char* arg = nextTok(&p);
    char* seqS = nextTok(&p);
    uint8_t seq = seqS ? (uint8_t)atoi(seqS) : 0;
    bool ok = testHookCallResponse(arg ? arg : "cancel", seq);
    Serial.printf("EVT CALL_RESP kind=%s seq=%u ok=%d\n", arg ? arg : "cancel", seq, ok ? 1 : 0);
    return;
  }

  // --- BLE ---
  if (strcmp(cmd, "BLE") == 0) {
    char* what = nextTok(&p);
    if (what && strcasecmp(what, "ADV") == 0) {
      char* arg = nextTok(&p);
      bool on = arg && strcasecmp(arg, "OFF") != 0;
      if (on) bleStartAdvertising(); else bleStopAdvertising();
      Serial.printf("EVT BLE_ADV state=%s\n", on ? "ON" : "OFF");
    } else if (what && strcasecmp(what, "STATS") == 0) {
      Serial.printf("EVT BLE_STATS conn=%lu disc=%lu last_reason=%d "
                    "notify_ok=%lu notify_fail=%lu notify_retry=%lu "
                    "notify_noconn=%lu connected=%d\n",
        (unsigned long)bleConnCount, (unsigned long)bleDiscCount, bleLastDiscReason,
        (unsigned long)bleNotifyOk, (unsigned long)bleNotifyFail,
        (unsigned long)bleNotifyRetry, (unsigned long)bleNotifyNoConn,
        bleIsConnected() ? 1 : 0);
    } else {
      Serial.printf("EVT BLE_STATE connected=%d name=%s pin=%04lu\n",
        bleIsConnected() ? 1 : 0, bleGetDeviceName().c_str(),
        (unsigned long)bleGetPin());
    }
    return;
  }

  Serial.printf("EVT ERR cmd=%s reason=unknown\n", cmd);
}

// ================================================================
// Публичное API
// ================================================================
void testConsoleInit() {
  lineLen = 0;
  statReset();
  Serial.println("EVT CONSOLE_READY");
}

void testConsoleTick() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = 0;
      if (lineLen > 0) handleLine(lineBuf);
      lineLen = 0;
      continue;
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;
  }
}

#endif // TEST_CONSOLE
