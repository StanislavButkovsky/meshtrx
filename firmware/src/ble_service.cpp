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
volatile uint32_t bleAdvRestarts = 0;
volatile uint32_t bleStaleLinks = 0;
volatile uint32_t bleLastRxMs = 0;
volatile uint32_t bleIdleDrops = 0;
volatile bool bleLinkAuthorized = false;
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
    bleLastRxMs = bleLastConnMs;
    bleLinkAuthorized = false;   // каждое соединение здоровается заново
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
    bleLastRxMs = millis();
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
#ifdef BOARD_V4
  // V4: керамическая антенна WiFi/BLE отдаёт заметно меньше, чем у V3, —
  // берём максимум, который допускает контроллер.
  NimBLEDevice::setPower(9);
#else
  NimBLEDevice::setPower(6);  // +6dBm
#endif

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

  // Advertising.
  //
  // Имя обязано быть в основном рекламном пакете. В нём всего 31 байт, из
  // которых три уходят на флаги, а 128-битный UUID сервиса съедает ещё
  // восемнадцать — на имя из двенадцати символов места уже не остаётся, и
  // устройство уходит в эфир безымянным. Телефон такое не показывает, а
  // приложение ищет как раз по имени: для человека рация просто «не видна».
  // Поэтому UUID переносим в scan response, а имя оставляем в основном пакете.
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(nameBuf);

  NimBLEAdvertisementData scanResponse;
  scanResponse.setCompleteServices(NimBLEUUID(SERVICE_UUID));
  pAdvertising->setScanResponseData(scanResponse);
  pAdvertising->enableScanResponse(true);

  pAdvertising->setMinInterval(160);        // 100 мс — интервал рекламы
  pAdvertising->setMaxInterval(320);        // 200 мс
  pAdvertising->setPreferredParams(24, 40); // предпочитаемое соединение 30-50 мс
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

bool bleIsAdvertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  return adv && adv->isAdvertising();
}

size_t bleConnectedCount() {
  return pServer ? pServer->getConnectedCount() : 0;
}

bool bleEnsureAdvertising() {
  // Устройство, которое не рекламируется и ни с кем не соединено, для
  // пользователя просто исчезает: телефон его не находит, а причину увидеть
  // неоткуда.
  //
  // Отдельно ловим залипший флаг: если телефон ушёл, не разорвав соединение,
  // колбэк onDisconnect может не прийти вовсе, и устройство молчит в эфире,
  // считая, что с ним кто-то работает. Настоящее число соединений знает стек —
  // на него и опираемся, а не на собственный флаг.
  if (bleConnected && bleConnectedCount() == 0) {
    bleConnected = false;
    bleStaleLinks++;
    Serial.printf("EVT BLE_STALE_LINK n=%lu\n", (unsigned long)bleStaleLinks);
  }
  // Соединение, в котором приложение не сказало ни слова, — брошенное.
  // Живой клиент здоровается сразу: шлёт PIN и запрашивает настройки. Пока
  // такое соединение висит, устройство не рекламируется, и для остальных
  // рация просто не существует, поэтому освобождаем канал сами.
  //
  // Авторизованное соединение не трогаем никогда: приложение может слушать
  // эфир часами, не отправляя ни одной команды, и обрывать его было бы хуже
  // самой болезни.
  if (bleConnected && !bleLinkAuthorized && bleLastRxMs &&
      millis() - bleLastRxMs > BLE_IDLE_LINK_TIMEOUT_MS) {
    bleIdleDrops++;
    Serial.printf("EVT BLE_IDLE_DROP n=%lu silent_ms=%lu\n",
      (unsigned long)bleIdleDrops, (unsigned long)(millis() - bleLastRxMs));
    if (pServer) {
      std::vector<uint16_t> handles = pServer->getPeerDevices();
      for (uint16_t h : handles) pServer->disconnect(h);
    }
    bleConnected = false;
    bleLastRxMs = millis();
  }

  if (bleConnected || bleIsAdvertising()) return false;
  NimBLEDevice::getAdvertising()->start();
  bleAdvRestarts++;
  Serial.printf("EVT BLE_ADV_RESTART n=%lu\n", (unsigned long)bleAdvRestarts);
  return true;
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
