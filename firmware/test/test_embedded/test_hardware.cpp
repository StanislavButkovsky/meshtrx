#include <Arduino.h>
#include <unity.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include "../../src/lora_radio.h"
#include "../../src/oled_display.h"
#include "../../src/battery.h"
#include "../../src/ble_service.h"
#include "../../src/beacon.h"
#include "../../src/utils.h"
#include "../../src/packet.h"

// ================================================================
// LoRa Radio Tests
// ================================================================

void test_lora_init() {
    loraInit();
    // Если дошли сюда — init не упал
    TEST_ASSERT_TRUE(true);
}

void test_lora_set_channel() {
    TEST_ASSERT_TRUE(loraSetChannel(0));    // первый канал
    TEST_ASSERT_TRUE(loraSetChannel(11));   // середина
    TEST_ASSERT_TRUE(loraSetChannel(22));   // последний канал
    TEST_ASSERT_FALSE(loraSetChannel(23));  // за пределами
    TEST_ASSERT_FALSE(loraSetChannel(255));
}

void test_lora_frequency() {
    float freq = loraGetFrequency(0);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 863.15, freq);  // первый канал
    float freq22 = loraGetFrequency(22);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 869.75, freq22); // последний канал
    TEST_ASSERT_EQUAL_FLOAT(0, loraGetFrequency(23)); // невалидный
}

void test_lora_tx_power() {
    loraSetTxPower(1);
    TEST_ASSERT_EQUAL(1, loraGetTxPower());
    loraSetTxPower(14);
    TEST_ASSERT_EQUAL(14, loraGetTxPower());
    loraSetTxPower(22);
    TEST_ASSERT_EQUAL(MAX_TX_POWER_DBM, loraGetTxPower());
}

void test_lora_start_receive() {
    TEST_ASSERT_TRUE(loraStartReceive());
}

void test_lora_send_small_packet() {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    bool ok = loraSend(data, sizeof(data));
    TEST_ASSERT_TRUE(ok);
    loraStartReceive();
}

void test_lora_power_mode_continuous() {
    loraSetPowerMode(LORA_POWER_CONTINUOUS_RX);
    TEST_ASSERT_EQUAL(LORA_POWER_CONTINUOUS_RX, loraGetPowerMode());
}

void test_lora_power_mode_sleep() {
    loraSetPowerMode(LORA_POWER_SLEEP);
    TEST_ASSERT_EQUAL(LORA_POWER_SLEEP, loraGetPowerMode());
    // Вернуть в RX
    loraSetPowerMode(LORA_POWER_CONTINUOUS_RX);
    TEST_ASSERT_EQUAL(LORA_POWER_CONTINUOUS_RX, loraGetPowerMode());
}

void test_lora_send_wake() {
    uint8_t data[] = {0xD0, 0x01, 0x02, 0x03, 0x04};
    bool ok = loraSendWake(data, sizeof(data));
    TEST_ASSERT_TRUE(ok);
    loraStartReceive();
}

void test_lora_mutex_exists() {
    TEST_ASSERT_NOT_NULL(loraRadioMutex);
}

// ================================================================
// OLED Display Tests
// ================================================================

void test_oled_init() {
    oledInit();
    TEST_ASSERT_TRUE(oledIsAwake());
}

void test_oled_sleep_wake() {
    oledOff();
    TEST_ASSERT_FALSE(oledIsAwake());
    oledWake();
    TEST_ASSERT_TRUE(oledIsAwake());
}

void test_oled_show_message() {
    oledShowMessage("TEST", "OK", 1000);
    // Не упало — ок
    TEST_ASSERT_TRUE(true);
}

// ================================================================
// Battery ADC Tests
// ================================================================

void test_battery_voltage_range() {
    float v = batteryReadVoltage();
    // Должно быть в диапазоне 3.0-4.5V (или 0 если нет батареи, USB питание)
    TEST_ASSERT_TRUE(v >= 0.0 && v <= 5.0);
}

void test_battery_percent() {
    int pct = batteryReadPercent();
    TEST_ASSERT_TRUE(pct >= 0 && pct <= 255);
}

// ================================================================
// BLE Tests
// ================================================================

void test_ble_init() {
    bleInit();
    // Не упало
    TEST_ASSERT_TRUE(true);
}

void test_ble_pin() {
    uint32_t pin = bleGetPin();
    TEST_ASSERT_TRUE(pin < 10000);  // 4-значный PIN
}

void test_ble_not_connected() {
    TEST_ASSERT_FALSE(bleIsConnected());  // при тесте нет подключённого телефона
}

// ================================================================
// Beacon Tests
// ================================================================

void test_beacon_init() {
    beaconInit();
    TEST_ASSERT_TRUE(true);
}

void test_beacon_callsign() {
    const char* cs = beaconGetCallSign();
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_TRUE(strlen(cs) > 0);  // есть позывной (хотя бы TX-XXXX)
}

void test_beacon_send() {
    bool ok = beaconSendNow();
    TEST_ASSERT_TRUE(ok);
    loraStartReceive();
}

// ================================================================
// WiFi Off Test
// ================================================================

void test_wifi_off() {
    WiFi.mode(WIFI_OFF);
    TEST_ASSERT_TRUE(true);  // не упало
}

// ================================================================
// Heap / Memory Tests
// ================================================================

void test_free_heap() {
    uint32_t heap = ESP.getFreeHeap();
    Serial.printf("  Free heap: %d bytes\n", heap);
    TEST_ASSERT_GREATER_THAN(100000, heap);  // > 100 КБ свободно
}

void test_malloc_200kb() {
    uint32_t heapBefore = ESP.getFreeHeap();
    uint8_t* buf = (uint8_t*)malloc(200 * 1024);
    if (buf) {
        memset(buf, 0xAA, 200 * 1024);  // записать для проверки
        free(buf);
        TEST_ASSERT_TRUE(true);
    } else {
        Serial.printf("  Heap before: %d, malloc 200KB failed\n", heapBefore);
        TEST_ASSERT_TRUE(heapBefore < 230000);  // ок если heap < 230 КБ
    }
}

// ================================================================
// GPIO Tests
// ================================================================

void test_gpio_led() {
    pinMode(35, OUTPUT);
    digitalWrite(35, HIGH);
    delay(100);
    digitalWrite(35, LOW);
    TEST_ASSERT_TRUE(true);
}

void test_gpio_button() {
    pinMode(0, INPUT_PULLUP);
    int val = digitalRead(0);
    TEST_ASSERT_EQUAL(HIGH, val);  // кнопка не нажата = HIGH (pullup)
}

void test_gpio_vext() {
    pinMode(36, OUTPUT);
    digitalWrite(36, LOW);   // вкл
    delay(50);
    digitalWrite(36, HIGH);  // выкл
    TEST_ASSERT_TRUE(true);
}

// ================================================================
// Main
// ================================================================

void test_cpu_160mhz() {
    setCpuFrequencyMhz(160);
    TEST_ASSERT_EQUAL(160, getCpuFrequencyMhz());
}

void setup() {
    // Понизить частоту ДО всех инициализаций
    setCpuFrequencyMhz(160);

    delay(2000);
    Serial.begin(115200);
    Serial.printf("\n=== CPU: %d MHz ===\n", getCpuFrequencyMhz());

    UNITY_BEGIN();

    // CPU
    RUN_TEST(test_cpu_160mhz);

    // GPIO
    RUN_TEST(test_gpio_led);
    RUN_TEST(test_gpio_button);
    RUN_TEST(test_gpio_vext);

    // OLED
    RUN_TEST(test_oled_init);
    RUN_TEST(test_oled_sleep_wake);
    RUN_TEST(test_oled_show_message);

    // Battery
    RUN_TEST(test_battery_voltage_range);
    RUN_TEST(test_battery_percent);

    // Heap
    RUN_TEST(test_free_heap);
    RUN_TEST(test_malloc_200kb);

    // WiFi
    RUN_TEST(test_wifi_off);

    // LoRa
    RUN_TEST(test_lora_init);
    RUN_TEST(test_lora_mutex_exists);
    RUN_TEST(test_lora_set_channel);
    RUN_TEST(test_lora_frequency);
    RUN_TEST(test_lora_tx_power);
    RUN_TEST(test_lora_start_receive);
    RUN_TEST(test_lora_power_mode_continuous);
    RUN_TEST(test_lora_power_mode_sleep);
    RUN_TEST(test_lora_send_small_packet);
    RUN_TEST(test_lora_send_wake);

    // BLE (после LoRa, т.к. NimBLE инициализирует радио)
    RUN_TEST(test_ble_init);
    RUN_TEST(test_ble_pin);
    RUN_TEST(test_ble_not_connected);

    // Beacon (после LoRa + BLE init)
    RUN_TEST(test_beacon_init);
    RUN_TEST(test_beacon_callsign);
    RUN_TEST(test_beacon_send);

    UNITY_END();
}

void loop() {
    // тесты завершены
}
