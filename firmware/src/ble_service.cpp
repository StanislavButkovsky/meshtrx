#include "ble_service.h"
#include "oled_display.h"
#include <esp_mac.h>

bool bleConnected = false;

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
    Serial.println("[BLE] Client connected");
    pServer->updateConnParams(connInfo.getConnHandle(), 48, 80, 2, 500);
    oledWake();
    oledShowMessage("BLE CONNECTED", "", 3000);
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    bleConnected = false;
    Serial.printf("[BLE] Client disconnected (reason=%d)\n", reason);
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
  pAdvertising->enableScanResponse(false);  // экономия ~1-2 мА
  pAdvertising->setPreferredParams(160, 320);  // 100-200мс interval
  pAdvertising->start();

  // PIN на OLED
  char pinMsg[16];
  snprintf(pinMsg, sizeof(pinMsg), "PIN: %04lu", (unsigned long)blePin);
  oledShowMessage(pinMsg, nameBuf, 10000);

  Serial.println("[BLE] Advertising started");
}

uint32_t bleGetPin() {
  return blePin;
}

bool bleSendNotify(uint8_t* data, size_t len) {
  if (!bleConnected || !pTxChar) return false;
  pTxChar->setValue(data, len);
  pTxChar->notify();
  return true;
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
