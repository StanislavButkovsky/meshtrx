#include "beacon.h"
#include "lora_radio.h"
#include "ble_service.h"
#include "oled_display.h"
#include "battery.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_mac.h>

static uint32_t beaconIntervalSec = BEACON_INTERVAL_5MIN;
static char callSign[9] = {0};
static uint16_t beaconSeqCounter = 0;

// Координаты от телефона
static int32_t currentLat = 0;
static int32_t currentLon = 0;
static int16_t currentAlt = 0x7FFF;
static bool currentGpsValid = false;

volatile bool locationRequested = false;
volatile bool locationReceived = false;

// Device ID (последние 4 байта MAC)
static uint8_t deviceId[4] = {0};

static void loadSettings() {
  Preferences prefs;
  prefs.begin("beacon", true);
  beaconIntervalSec = prefs.getUInt("interval", BEACON_INTERVAL_5MIN);
  String cs = prefs.getString("callsign", "");
  if (cs.length() > 0) {
    strncpy(callSign, cs.c_str(), 8);
  }
  prefs.end();
}

static void saveSettings() {
  Preferences prefs;
  prefs.begin("beacon", false);
  prefs.putUInt("interval", beaconIntervalSec);
  prefs.putString("callsign", callSign);
  prefs.end();
}

// Батарея — используем battery.h (единая функция)

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

void beaconInit() {
  // Получить device ID
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  memcpy(deviceId, mac + 2, 4);

  loadSettings();

  // Если позывной пуст — сгенерировать из MAC
  if (callSign[0] == 0) {
    snprintf(callSign, sizeof(callSign), "TX-%02X%02X", mac[4], mac[5]);
    saveSettings();
  }

  Serial.printf("[Beacon] Interval: %d sec, CallSign: %s, ID: %02X%02X%02X%02X\n",
    beaconIntervalSec, callSign,
    deviceId[0], deviceId[1], deviceId[2], deviceId[3]);
}

void beaconSetInterval(uint32_t seconds) {
  beaconIntervalSec = seconds;
  saveSettings();
  Serial.printf("[Beacon] Interval → %d sec\n", seconds);
}

uint32_t beaconGetInterval() {
  return beaconIntervalSec;
}

void beaconSetCallSign(const char* cs) {
  strncpy(callSign, cs, 8);
  callSign[8] = 0;
  saveSettings();
}

const char* beaconGetCallSign() {
  return callSign;
}

int32_t beaconGetLat() { return currentLat; }
int32_t beaconGetLon() { return currentLon; }

void beaconUpdateLocation(int32_t lat_e7, int32_t lon_e7, int16_t alt_m, bool gpsValid) {
  currentLat = lat_e7;
  currentLon = lon_e7;
  currentAlt = alt_m;
  currentGpsValid = gpsValid;
  locationReceived = true;
}

bool beaconSendNow() {
  LoRaBeaconPacket pkt;
  memset(&pkt, 0, sizeof(pkt));

  pkt.type = PKT_TYPE_BEACON;
  pkt.channel = loraGetChannel();
  memcpy(pkt.device_id, deviceId, 4);
  memcpy(pkt.call_sign, callSign, 9);
  pkt.lat_e7 = currentLat;
  pkt.lon_e7 = currentLon;
  pkt.altitude_m = currentAlt;
  pkt.tx_power = loraGetTxPower();
  pkt.battery = batteryReadPercent();
  pkt.flags = 0;
  if (currentGpsValid) pkt.flags |= BEACON_FLAG_GPS_VALID;
  pkt.uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);
  pkt.beacon_seq = beaconSeqCounter++;

  // CRC16 от байтов 0..33 (всё кроме самого crc16)
  pkt.crc16 = crc16_ccitt((uint8_t*)&pkt, sizeof(pkt) - 2);

  bool ok = loraSendWake((uint8_t*)&pkt, sizeof(pkt));  // длинная преамбула — будит спящих
  if (ok) {
    Serial.printf("[Beacon] Sent seq=%d\n", pkt.beacon_seq);
    // Уведомить телефон
    uint8_t notif[3] = {BLE_CMD_BEACON_SENT,
      (uint8_t)(pkt.beacon_seq & 0xFF), (uint8_t)(pkt.beacon_seq >> 8)};
    bleSendNotify(notif, 3);
  }
  return ok;
}

void beaconProcessIncoming(const LoRaBeaconPacket* pkt, int16_t rssi, int8_t snr) {
  // Проверить CRC
  uint16_t calcCrc = crc16_ccitt((const uint8_t*)pkt, sizeof(LoRaBeaconPacket) - 2);
  if (calcCrc != pkt->crc16) {
    Serial.println("[Beacon] CRC mismatch, drop");
    return;
  }

  Serial.printf("[Beacon] SEEN: %s RSSI:%d SNR:%d\n",
    pkt->call_sign, rssi, snr);

  // Показать на OLED (только если экран уже включён)
  char buf[22];
  snprintf(buf, sizeof(buf), "SEEN: %s %ddBm", pkt->call_sign, rssi);
  oledShowMessage(buf, "", 2000);

  // Отправить PEER_SEEN на телефон (0x17)
  uint8_t bleData[32];
  bleData[0] = BLE_CMD_PEER_SEEN;
  memcpy(bleData + 1, pkt->device_id, 4);
  memcpy(bleData + 5, pkt->call_sign, 9);
  memcpy(bleData + 14, &pkt->lat_e7, 4);
  memcpy(bleData + 18, &pkt->lon_e7, 4);
  int16_t rssiVal = rssi;
  memcpy(bleData + 22, &rssiVal, 2);
  bleData[24] = (uint8_t)snr;
  bleData[25] = pkt->tx_power;
  bleData[26] = pkt->battery;
  bleData[27] = pkt->flags;
  memcpy(bleData + 28, &pkt->uptime_sec, 4);
  bleSendNotify(bleData, 32);
}

void beaconTask(void* param) {
  // Случайная начальная задержка 5-15 сек (антиколлизия)
  vTaskDelay(pdMS_TO_TICKS(5000 + (esp_random() % 10000)));

  while (true) {
    if (beaconIntervalSec == BEACON_INTERVAL_NEVER) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    // Запросить координаты от телефона
    if (bleIsConnected()) {
      locationReceived = false;
      locationRequested = true;
      uint8_t cmd = BLE_CMD_GET_LOCATION;
      bleSendNotify(&cmd, 1);

      // Ждать ответ до 2 секунд
      uint32_t start = millis();
      while (!locationReceived && (millis() - start) < 2000) {
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      locationRequested = false;
    }

    // Отправить beacon
    beaconSendNow();
    loraStartReceive();

    // Jitter ±15% от интервала
    uint32_t jitter = beaconIntervalSec * 150; // мс * 0.15
    uint32_t waitMs = beaconIntervalSec * 1000;
    waitMs += (esp_random() % (jitter * 2)) - jitter;
    vTaskDelay(pdMS_TO_TICKS(waitMs));
  }
}
