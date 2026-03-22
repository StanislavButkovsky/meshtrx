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
bool beaconSendNow();

// Обработка входящего beacon от другого устройства
void beaconProcessIncoming(const LoRaBeaconPacket* pkt, int16_t rssi, int8_t snr);

// CRC16-CCITT
uint16_t crc16_ccitt(const uint8_t* data, size_t len);

// Задача FreeRTOS
void beaconTask(void* param);

// Запрос координат от телефона (ожидание ответа)
extern volatile bool locationRequested;
extern volatile bool locationReceived;
