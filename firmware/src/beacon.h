#pragma once
#include <stdint.h>
#include <stddef.h>
#include "packet.h"

void beaconInit();
void beaconSetInterval(uint32_t seconds);
uint32_t beaconGetInterval();
void beaconSetCallSign(const char* callSign);
const char* beaconGetCallSign();

// Обновить координаты от телефона
void beaconUpdateLocation(int32_t lat_e7, int32_t lon_e7, int16_t alt_m, bool gpsValid);

// Получить текущие координаты
int32_t beaconGetLat();
int32_t beaconGetLon();

// Собрать и отправить beacon пакет
bool beaconSendNow(bool request = false);

// Обработка входящего beacon от другого устройства
void beaconProcessIncoming(const LoRaBeaconPacket* pkt, int16_t rssi, int8_t snr);

// CRC16-CCITT — определён в utils.h
#include "utils.h"

// Задача FreeRTOS
void beaconTask(void* param);

// Не отвечать на запросы чаще, чем раз в это время
#define BEACON_REPLY_MIN_GAP_MS 20000

// Попросить соседей отозваться — их маяки придут за секунды
void beaconRequestPeers();
// Ответить на чужой запрос присутствия (со случайной задержкой)
void beaconScheduleReply();

// Запрос координат от телефона (ожидание ответа)
extern volatile bool locationRequested;
extern volatile bool locationReceived;
