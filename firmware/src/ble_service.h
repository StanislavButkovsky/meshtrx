#pragma once
#include <NimBLEDevice.h>

// === BLE UUIDs (Nordic UART Service) ===
#define SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_CHAR_UUID  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // телефон→ESP32
#define TX_CHAR_UUID  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP32→телефон

// === BLE команды ===
#define BLE_CMD_AUDIO_TX      0x01  // телефон→ESP: аудио для LoRa
#define BLE_CMD_AUDIO_RX      0x02  // ESP→телефон: аудио из LoRa
#define BLE_CMD_PTT_START     0x03
#define BLE_CMD_PTT_END       0x04
#define BLE_CMD_SET_CHANNEL   0x05
#define BLE_CMD_STATUS_UPDATE 0x06  // ESP→телефон
#define BLE_CMD_SEND_MESSAGE  0x07
#define BLE_CMD_RECV_MESSAGE  0x08  // ESP→телефон
#define BLE_CMD_MESSAGE_ACK   0x09  // ESP→телефон
#define BLE_CMD_SET_SETTINGS  0x0A
#define BLE_CMD_GET_SETTINGS  0x0B
#define BLE_CMD_SETTINGS_RESP 0x0C  // ESP→телефон
#define BLE_CMD_FILE_START    0x0D
#define BLE_CMD_FILE_CHUNK    0x0E
#define BLE_CMD_FILE_RECV     0x0F  // ESP→телефон
#define BLE_CMD_FILE_PROGRESS 0x10  // ESP→телефон
#define BLE_CMD_SET_TX_MODE   0x11
#define BLE_CMD_VOX_STATUS    0x12  // ESP→телефон
#define BLE_CMD_VOX_LEVEL     0x13
#define BLE_CMD_GET_LOCATION  0x14  // ESP→телефон
#define BLE_CMD_LOCATION_UPD  0x15
#define BLE_CMD_BEACON_SENT   0x16  // ESP→телефон
#define BLE_CMD_PEER_SEEN     0x17  // ESP→телефон
#define BLE_CMD_CALL_ALL      0x18
#define BLE_CMD_CALL_PRIVATE  0x19
#define BLE_CMD_CALL_GROUP    0x1A
#define BLE_CMD_CALL_EMERGENCY 0x1B
#define BLE_CMD_CALL_ACCEPT   0x1C
#define BLE_CMD_CALL_REJECT   0x1D
#define BLE_CMD_CALL_CANCEL   0x1E
#define BLE_CMD_INCOMING_CALL 0x1F  // ESP→телефон
#define BLE_CMD_CALL_STATUS   0x20  // ESP→телефон
#define BLE_CMD_GROUP_LIST    0x21
#define BLE_CMD_GROUP_LIST_RESP 0x22 // ESP→телефон
#define BLE_CMD_GROUP_SAVE    0x23
#define BLE_CMD_GROUP_DELETE  0x24
#define BLE_CMD_PIN_CHECK     0x25  // телефон→ESP: 4 байта PIN (uint32 LE)
#define BLE_CMD_PIN_RESULT    0x26  // ESP→телефон: байт 1: 0=FAIL, 1=OK
#define BLE_CMD_FILE_DATA     0x27  // ESP→телефон: чанк данных принятого файла
#define BLE_CMD_SET_REPEATER  0x28  // телефон→ESP: [enable, ssid...\0, pass...\0]

extern bool bleConnected;

void bleInit();
bool bleSendNotify(uint8_t* data, size_t len);
bool bleIsConnected();
String bleGetDeviceName();
uint32_t bleGetPin();

// Callback — вызывается при получении данных от телефона
typedef void (*BleDataCallback)(uint8_t* data, size_t len);
void bleSetDataCallback(BleDataCallback cb);
