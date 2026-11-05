#pragma once
#include <Arduino.h>

// Связь с телефоном или компьютером по кабелю (OTG на телефоне, USB на
// компьютере). По проводу идут те же пакеты, что по Bluetooth, только в рамках:
//   0xC1  SLIP(пакет + CRC-16)  0xC0
// Байты 0xC0 и 0xC1 не встречаются в тексте UTF-8, поэтому рамки не путаются
// со служебным журналом, который прошивка пишет в тот же порт. Внутри рамки
// 0xC0, 0xC1 и 0xDB экранируются через 0xDB.

typedef void (*UsbLinkDataCallback)(uint8_t* data, size_t len);

void usbLinkInit(UsbLinkDataCallback cb);
// Читать порт: разбирает рамки, всё остальное отдаёт тестовой консоли.
void usbLinkPoll();
// По кабелю кто-то есть: за последние USB_LINK_TIMEOUT_MS пришла целая рамка.
bool usbLinkActive();
bool usbLinkSend(const uint8_t* data, size_t len);

#ifdef TEST_CONSOLE
// Текст вне рамок — команды стенда; консоль забирает его отсюда.
int usbLinkConsoleRead();
#endif

#define USB_LINK_TIMEOUT_MS 15000
