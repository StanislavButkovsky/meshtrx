#include "ble_service.h"
#include "oled_display.h"
#include "lora_radio.h"
#include <esp_mac.h>

bool bleConnected = false;
volatile bool bleConnEvent = false;  // обработать в bleTask, не в колбэке
volatile uint32_t bleConnCount = 0;
volatile uint32_t bleDiscCount = 0;
volatile int      bleLastDiscReason = 0;
volatile uint32_t bleNotifyOk = 0;
volatile uint32_t bleNotifyFail = 0;
volatile uint32_t bleNotifyNoConn = 0;
volatile uint32_t bleNotifyRetry = 0;
volatile uint32_t bleLastConnMs = 0;
volatile uint32_t bleLastDiscMs = 0;

static NimBLEServer* pServer = nullptr;
static NimBLECharacteristic* pTxChar = nullptr;
static NimBLECharacteristic* pRxChar = nullptr;
static BleDataCallback dataCallback = nullptr;
static String deviceName;
static uint32_t blePin = 0;

// === Server callbacks ===
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    bleConnected = true;
    bleConnCount++;
    bleLastConnMs = millis();
    Serial.printf("EVT BLE_CONN n=%lu gap_ms=%lu\n",
      (unsigned long)bleConnCount,
      (unsigned long)(bleLastDiscMs ? bleLastConnMs - bleLastDiscMs : 0));
    // 30-50 мс, slave latency 0 (аудио идёт каждые 80 мс), supervision timeout 5 с
    pServer->updateConnParams(connInfo.getConnHandle(), 24, 40, 0, 500);
    if (loraTaskHandle) xTaskNotifyGive(loraTaskHandle);
    // OLED не трогаем здесь: это контекст хост-задачи NimBLE, блокировать его нельзя
    bleConnEvent = true;
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    bleConnected = false;
    bleDiscCount++;
    bleLastDiscReason = reason;
    bleLastDiscMs = millis();
    Serial.printf("EVT BLE_DISC reason=%d n=%lu held_ms=%lu\n",
      reason, (unsigned long)bleDiscCount,
      (unsigned long)(bleLastConnMs ? bleLastDiscMs - bleLastConnMs : 0));
  }

  void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
    Serial.printf("[BLE] MTU changed to %d\n", MTU);
  }
};

// === RX characteristic callbacks ===
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    NimBLEAttValue val = pCharacteristic->getValue();
    if (val.length() > 0 && dataCallback) {
      dataCallback((uint8_t*)val.data(), val.length());
    }
  }
};

void bleInit() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char nameBuf[20];
  snprintf(nameBuf, sizeof(nameBuf), "MeshTRX-%02X%02X", mac[4], mac[5]);
  deviceName = String(nameBuf);

  // PIN из MAC — показывается на OLED
  blePin = ((uint32_t)mac[4] * 256 + mac[5]) % 10000;
  Serial.printf("[BLE] Device: %s  PIN: %04lu\n", nameBuf, (unsigned long)blePin);

  NimBLEDevice::init(nameBuf);
  NimBLEDevice::setMTU(128);
  NimBLEDevice::setPower(6);  // +6dBm

  // Без BLE security — PIN проверяется на уровне приложения
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  pServer->advertiseOnDisconnect(true);  // NimBLE 2.x: auto-restart advertising

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  // TX char (ESP32→телефон): Notify
  pTxChar = pService->createCharacteristic(
    TX_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  // RX char (телефон→ESP32): Write
  pRxChar = pService->createCharacteristic(
    RX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pRxChar->setCallbacks(new RxCallbacks());

  pService->start();

  // Advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->enableScanResponse(true);   // имя устройства в scan response — надёжное обнаружение
  pAdvertising->setMinInterval(160);        // 100 мс — интервал рекламы
  pAdvertising->setMaxInterval(320);        // 200 мс
  pAdvertising->setPreferredParams(24, 40); // предпочитаемое соединение 30-50 мс (было 1000-2000!)
  pAdvertising->start();

  // PIN на OLED
  char pinMsg[16];
  snprintf(pinMsg, sizeof(pinMsg), "PIN: %04lu", (unsigned long)blePin);
  oledShowMessage(pinMsg, nameBuf, 10000);

  Serial.println("[BLE] Advertising started");
}

void bleStartAdvertising() {
  NimBLEDevice::getAdvertising()->start();
  Serial.println("[BLE] Advertising started");
}

void bleStopAdvertising() {
  NimBLEDevice::getAdvertising()->stop();
  Serial.println("[BLE] Advertising stopped");
}

uint32_t bleGetPin() {
  return blePin;
}

bool bleSendNotify(uint8_t* data, size_t len) {
  // Телефона нет — это не потеря данных, а обычное состояние; считаем отдельно,
  // иначе статистика notify_fail врёт каждый раз, когда никто не подключён.
  if (!bleConnected || !pTxChar) { bleNotifyNoConn++; return false; }

  pTxChar->setValue(data, len);
  if (pTxChar->notify()) { bleNotifyOk++; return true; }

  // Единственная реальная причина отказа при живом соединении — переполнение
  // очереди контроллера. Ждём один интервал соединения и пробуем ещё раз:
  // голос идёт 12 пакетов в секунду, потерять кадр хуже, чем задержать его.
  vTaskDelay(pdMS_TO_TICKS(20));
  if (!bleConnected) { bleNotifyNoConn++; return false; }
  pTxChar->setValue(data, len);
  if (pTxChar->notify()) { bleNotifyOk++; bleNotifyRetry++; return true; }

  bleNotifyFail++;
  return false;
}

bool bleIsConnected() {
  return bleConnected;
}

String bleGetDeviceName() {
  return deviceName;
}

void bleSetDataCallback(BleDataCallback cb) {
  dataCallback = cb;
}
