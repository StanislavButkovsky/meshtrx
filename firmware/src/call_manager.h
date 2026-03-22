#pragma once
#include <stdint.h>
#include <stddef.h>
#include "packet.h"

// === Состояния вызова ===
enum CallState {
  CALL_IDLE,
  CALL_OUTGOING,
  CALL_INCOMING,
  CALL_ACTIVE,
  CALL_EMERGENCY_TX,
  CALL_EMERGENCY_RX
};

void callManagerInit();
CallState callGetState();

// Обработка входящих LoRa пакетов вызовов
void callProcessAllCall(const LoRaCallAll* pkt, int16_t rssi, uint8_t ttl);
void callProcessPrivateCall(const LoRaCallPrivate* pkt, int16_t rssi, uint8_t ttl);
void callProcessGroupCall(const LoRaCallGroup* pkt, int16_t rssi, uint8_t ttl);
void callProcessEmergency(const LoRaCallEmergency* pkt, int16_t rssi, uint8_t ttl);
void callProcessResponse(const LoRaCallResponse* pkt);

// Отправка вызовов (из BLE команд)
void callSendAll(const uint8_t* message, size_t msgLen);
void callSendPrivate(const uint8_t* targetId, const uint8_t* message, size_t msgLen);
void callSendGroup(uint8_t groupIndex, const uint8_t* adhocMembers, uint8_t memberCount);
void callSendEmergency(int32_t lat_e7, int32_t lon_e7);

void callAccept(uint8_t callSeq);
void callReject(uint8_t callSeq);
void callCancel();

// Получить device_id
const uint8_t* callGetDeviceId();

// Таймер — вызывать из main loop
void callTick();
