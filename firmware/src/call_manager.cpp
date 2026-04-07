#include "call_manager.h"
#include "lora_radio.h"
#include <esp_mac.h>
#include "ble_service.h"
#include "oled_display.h"
#include "beacon.h"
#include <Arduino.h>
#include <esp_mac.h>

static CallState callState = CALL_IDLE;
static uint8_t deviceId[4] = {0};
static uint8_t callSeqCounter = 0;
static uint8_t activeCallSeq = 0;
static uint32_t callTimerStart = 0;
static uint8_t sosSentCount = 0;
static int32_t sosLat = 0, sosLon = 0;

void callManagerInit() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  memcpy(deviceId, mac + 2, 4);
  Serial.printf("[Call] Device ID: %02X%02X%02X%02X\n",
    deviceId[0], deviceId[1], deviceId[2], deviceId[3]);
}

CallState callGetState() {
  return callState;
}

const uint8_t* callGetDeviceId() {
  return deviceId;
}

// Уведомить телефон о входящем вызове (0x1F)
static void notifyIncomingCall(uint8_t callType, const uint8_t* senderId,
                                const uint8_t* callSignStr, uint8_t callSeq,
                                int32_t lat_e7 = 0, int32_t lon_e7 = 0) {
  uint8_t data[24];
  data[0] = BLE_CMD_INCOMING_CALL;
  data[1] = callType;
  memcpy(data + 2, senderId, 4);
  memcpy(data + 6, callSignStr, 9);
  memcpy(data + 15, &lat_e7, 4);
  memcpy(data + 19, &lon_e7, 4);
  data[23] = callSeq;
  bleSendNotify(data, 24);
}

// Уведомить телефон о статусе вызова (0x20)
static void notifyCallStatus(uint8_t status, const uint8_t* responderId) {
  uint8_t data[6];
  data[0] = BLE_CMD_CALL_STATUS;
  data[1] = status;
  if (responderId) {
    memcpy(data + 2, responderId, 4);
  } else {
    memset(data + 2, 0, 4);
  }
  bleSendNotify(data, 6);
}

void callProcessAllCall(const LoRaCallAll* pkt, int16_t rssi, uint8_t ttl) {
  Serial.printf("[Call] ALL CALL from %s\n", pkt->call_sign);
  notifyIncomingCall(0, pkt->sender, pkt->call_sign, 0, pkt->lat_e7, pkt->lon_e7);
  oledShowCallIncoming("ALL CALL", (const char*)pkt->call_sign,
                       "", rssi, ttl);
}

void callProcessPrivateCall(const LoRaCallPrivate* pkt, int16_t rssi, uint8_t ttl) {
  // Проверить: адресовано нам?
  if (memcmp(pkt->target, deviceId, 4) != 0) {
    // Не нам — тихое уведомление
    return;
  }
  Serial.printf("[Call] PRIVATE CALL from %s, seq=%d\n", pkt->call_sign, pkt->call_seq);
  callState = CALL_INCOMING;
  activeCallSeq = pkt->call_seq;
  notifyIncomingCall(1, pkt->sender, pkt->call_sign, pkt->call_seq, pkt->lat_e7, pkt->lon_e7);
  oledShowCallIncoming("CALL", (const char*)pkt->call_sign,
                       "", rssi, ttl);
}

void callProcessGroupCall(const LoRaCallGroup* pkt, int16_t rssi, uint8_t ttl) {
  // Проверить: наш device_id в members[]?
  bool forUs = false;
  for (int i = 0; i < pkt->member_count && i < MAX_GROUP_MEMBERS; i++) {
    if (memcmp(pkt->members[i], deviceId, 4) == 0) {
      forUs = true;
      break;
    }
  }
  if (!forUs) return;

  Serial.printf("[Call] GROUP CALL from %s, group=%s\n", pkt->call_sign, pkt->group_name);
  notifyIncomingCall(2, pkt->sender, pkt->call_sign, 0, pkt->lat_e7, pkt->lon_e7);
  oledShowCallIncoming("GROUP", (const char*)pkt->call_sign,
                       (const char*)pkt->group_name, rssi, ttl);
}

void callProcessEmergency(const LoRaCallEmergency* pkt, int16_t rssi, uint8_t ttl) {
  Serial.printf("[Call] EMERGENCY from %s!\n", pkt->call_sign);
  callState = CALL_EMERGENCY_RX;
  notifyIncomingCall(3, pkt->sender, pkt->call_sign, pkt->sos_seq,
                     pkt->lat_e7, pkt->lon_e7);
  oledShowSosIncoming((const char*)pkt->call_sign, pkt->lat_e7, pkt->lon_e7, rssi, ttl);
}

void callProcessResponse(const LoRaCallResponse* pkt) {
  if (callState != CALL_OUTGOING) return;
  if (pkt->call_seq != activeCallSeq) return;

  uint8_t status;
  if (pkt->type == PKT_TYPE_CALL_ACCEPT) {
    callState = CALL_ACTIVE;
    status = 1; // ACCEPTED
    Serial.println("[Call] ACCEPTED");
  } else if (pkt->type == PKT_TYPE_CALL_REJECT) {
    callState = CALL_IDLE;
    status = 2; // REJECTED
    Serial.println("[Call] REJECTED");
  } else if (pkt->type == PKT_TYPE_CALL_CANCEL) {
    callState = CALL_IDLE;
    status = 4; // CANCELLED
    Serial.println("[Call] CANCELLED by remote");
  } else {
    return;
  }

  notifyCallStatus(status, pkt->sender);
}

void callSendAll(const uint8_t* message, size_t msgLen) {
  LoRaCallAll pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_CALL_ALL;
  pkt.channel = loraGetChannel();
  pkt.ttl = TTL_DEFAULT;
  memcpy(pkt.sender, deviceId, 4);
  memcpy(pkt.call_sign, beaconGetCallSign(), 9);
  pkt.lat_e7 = beaconGetLat();
  pkt.lon_e7 = beaconGetLon();
  loraSendWake((uint8_t*)&pkt, sizeof(pkt));  // длинная преамбула — будит спящих
  loraStartReceive();
  Serial.println("[Call] ALL CALL sent");
}

void callSendPrivate(const uint8_t* targetId, const uint8_t* message, size_t msgLen) {
  LoRaCallPrivate pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_CALL_PRIVATE;
  pkt.channel = loraGetChannel();
  pkt.ttl = TTL_DEFAULT;
  memcpy(pkt.sender, deviceId, 4);
  memcpy(pkt.target, targetId, 4);
  memcpy(pkt.call_sign, beaconGetCallSign(), 9);
  pkt.call_seq = ++callSeqCounter;
  activeCallSeq = pkt.call_seq;
  pkt.lat_e7 = beaconGetLat();
  pkt.lon_e7 = beaconGetLon();

  loraSendWake((uint8_t*)&pkt, sizeof(pkt));  // длинная преамбула — будит спящих
  loraStartReceive();
  callState = CALL_OUTGOING;
  callTimerStart = millis();
  Serial.printf("[Call] PRIVATE sent to target, seq=%d\n", pkt.call_seq);
}

void callSendGroup(uint8_t groupIndex, const uint8_t* adhocMembers, uint8_t memberCount) {
  LoRaCallGroup pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_CALL_GROUP;
  pkt.channel = loraGetChannel();
  pkt.ttl = TTL_DEFAULT;
  memcpy(pkt.sender, deviceId, 4);
  pkt.group_id = (groupIndex == 0xFF) ? 0xFFFF : groupIndex;
  pkt.member_count = memberCount > MAX_GROUP_MEMBERS ? MAX_GROUP_MEMBERS : memberCount;
  if (adhocMembers) {
    memcpy(pkt.members, adhocMembers, pkt.member_count * 4);
  }
  memcpy(pkt.call_sign, beaconGetCallSign(), 9);
  pkt.lat_e7 = beaconGetLat();
  pkt.lon_e7 = beaconGetLon();

  loraSendWake((uint8_t*)&pkt, sizeof(pkt));  // длинная преамбула
  loraStartReceive();
  Serial.printf("[Call] GROUP sent, %d members\n", pkt.member_count);
}

void callSendEmergency(int32_t lat_e7, int32_t lon_e7) {
  LoRaCallEmergency pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_CALL_EMERGENCY;
  pkt.channel = loraGetChannel();
  pkt.ttl = TTL_MAX;
  memcpy(pkt.sender, deviceId, 4);
  memcpy(pkt.call_sign, beaconGetCallSign(), 9);
  pkt.lat_e7 = lat_e7;
  pkt.lon_e7 = lon_e7;
  pkt.sos_seq = ++callSeqCounter;
  pkt.flags = (lat_e7 != 0 || lon_e7 != 0) ? BEACON_FLAG_GPS_VALID : 0;
  memcpy(pkt.message, "SOS!", 4);

  sosLat = lat_e7;
  sosLon = lon_e7;
  sosSentCount = 1;

  loraSendWake((uint8_t*)&pkt, sizeof(pkt));  // длинная преамбула
  loraStartReceive();
  callState = CALL_EMERGENCY_TX;
  callTimerStart = millis();
  Serial.println("[Call] EMERGENCY SOS sent!");
}

void callAccept(uint8_t callSeq) {
  LoRaCallResponse pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_CALL_ACCEPT;
  pkt.channel = loraGetChannel();
  pkt.ttl = TTL_DEFAULT;
  memcpy(pkt.sender, deviceId, 4);
  pkt.call_seq = callSeq;

  loraSend((uint8_t*)&pkt, sizeof(pkt));
  loraStartReceive();
  callState = CALL_ACTIVE;
  Serial.printf("[Call] ACCEPT sent, seq=%d\n", callSeq);
}

void callReject(uint8_t callSeq) {
  LoRaCallResponse pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = PKT_TYPE_CALL_REJECT;
  pkt.channel = loraGetChannel();
  pkt.ttl = TTL_DEFAULT;
  memcpy(pkt.sender, deviceId, 4);
  pkt.call_seq = callSeq;

  loraSend((uint8_t*)&pkt, sizeof(pkt));
  loraStartReceive();
  callState = CALL_IDLE;
  Serial.printf("[Call] REJECT sent, seq=%d\n", callSeq);
}

void callCancel() {
  if (callState == CALL_EMERGENCY_TX) {
    callState = CALL_IDLE;
    sosSentCount = 0;
    Serial.println("[Call] SOS cancelled");
  } else if (callState == CALL_OUTGOING) {
    LoRaCallResponse pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PKT_TYPE_CALL_CANCEL;
    pkt.channel = loraGetChannel();
    pkt.ttl = TTL_DEFAULT;
    memcpy(pkt.sender, deviceId, 4);
    pkt.call_seq = activeCallSeq;

    loraSend((uint8_t*)&pkt, sizeof(pkt));
    loraStartReceive();
    callState = CALL_IDLE;
    Serial.println("[Call] CANCEL sent");
  } else {
    callState = CALL_IDLE;
  }
}

void callTick() {
  if (callState == CALL_OUTGOING) {
    if ((millis() - callTimerStart) > (CALL_TIMEOUT_SEC * 1000)) {
      callState = CALL_IDLE;
      notifyCallStatus(3, nullptr); // TIMEOUT
      Serial.println("[Call] TIMEOUT — no answer");
    }
  }

  if (callState == CALL_EMERGENCY_TX) {
    if ((millis() - callTimerStart) > (EMERGENCY_REPEAT_SEC * 1000)) {
      // Автоповтор SOS
      callSendEmergency(sosLat, sosLon);
      sosSentCount++;
      uint8_t countdown = EMERGENCY_REPEAT_SEC;
      oledShowSosActive(sosSentCount, countdown);
    }
  }
}
