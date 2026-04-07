#include <unity.h>
#include "../../src/utils.h"
#include "../../src/packet.h"

// ================================================================
// CRC16-CCITT Tests
// ================================================================

void test_crc16_empty() {
    uint16_t crc = crc16_ccitt(NULL, 0);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, crc);
}

void test_crc16_single_zero() {
    uint8_t data[] = {0x00};
    uint16_t crc = crc16_ccitt(data, 1);
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);
    TEST_ASSERT_NOT_EQUAL(0x0000, crc);
}

void test_crc16_known_vector() {
    // Standard CRC16-CCITT test vector: "123456789" → 0x29B1
    uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    uint16_t crc = crc16_ccitt(data, 9);
    TEST_ASSERT_EQUAL_UINT16(0x29B1, crc);
}

void test_crc16_deterministic() {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint16_t crc1 = crc16_ccitt(data, 4);
    uint16_t crc2 = crc16_ccitt(data, 4);
    TEST_ASSERT_EQUAL_UINT16(crc1, crc2);
}

void test_crc16_different_data_different_crc() {
    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x01, 0x02, 0x04};
    uint16_t crc1 = crc16_ccitt(data1, 3);
    uint16_t crc2 = crc16_ccitt(data2, 3);
    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

void test_crc16_large_buffer() {
    uint8_t data[1024];
    memset(data, 0x55, sizeof(data));
    uint16_t crc = crc16_ccitt(data, sizeof(data));
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);
}

// ================================================================
// Bitmap Tests
// ================================================================

void test_bitmap_clear() {
    uint8_t bitmap[128];
    memset(bitmap, 0xFF, sizeof(bitmap));
    bitmap_clear(bitmap, sizeof(bitmap));
    for (int i = 0; i < 128; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, bitmap[i]);
    }
}

void test_bitmap_set_and_get() {
    uint8_t bitmap[128] = {0};
    TEST_ASSERT_FALSE(bitmap_get(bitmap, 0));
    TEST_ASSERT_TRUE(bitmap_set(bitmap, 0));  // first time → true
    TEST_ASSERT_TRUE(bitmap_get(bitmap, 0));
    TEST_ASSERT_FALSE(bitmap_set(bitmap, 0)); // second time → false (duplicate)
}

void test_bitmap_various_positions() {
    uint8_t bitmap[128] = {0};
    uint16_t positions[] = {0, 1, 7, 8, 15, 16, 100, 500, 1023};
    for (int i = 0; i < 9; i++) {
        TEST_ASSERT_TRUE(bitmap_set(bitmap, positions[i]));
        TEST_ASSERT_TRUE(bitmap_get(bitmap, positions[i]));
    }
    // Check unset positions
    TEST_ASSERT_FALSE(bitmap_get(bitmap, 2));
    TEST_ASSERT_FALSE(bitmap_get(bitmap, 99));
    TEST_ASSERT_FALSE(bitmap_get(bitmap, 1022));
}

void test_bitmap_no_duplicates() {
    uint8_t bitmap[128] = {0};
    uint16_t unique = 0;
    for (int attempt = 0; attempt < 5; attempt++) {
        if (bitmap_set(bitmap, 42)) unique++;
    }
    TEST_ASSERT_EQUAL_UINT16(1, unique);
}

void test_bitmap_find_missing_none() {
    uint8_t bitmap[128] = {0};
    // Set all 10 chunks
    for (uint16_t i = 0; i < 10; i++) {
        bitmap_set(bitmap, i);
    }
    uint16_t missing[50];
    uint16_t cnt = bitmap_find_missing(bitmap, 10, missing, 50);
    TEST_ASSERT_EQUAL_UINT16(0, cnt);
}

void test_bitmap_find_missing_some() {
    uint8_t bitmap[128] = {0};
    // Set chunks 0, 2, 4 (skip 1, 3)
    bitmap_set(bitmap, 0);
    bitmap_set(bitmap, 2);
    bitmap_set(bitmap, 4);
    uint16_t missing[50];
    uint16_t cnt = bitmap_find_missing(bitmap, 5, missing, 50);
    TEST_ASSERT_EQUAL_UINT16(2, cnt);
    TEST_ASSERT_EQUAL_UINT16(1, missing[0]);
    TEST_ASSERT_EQUAL_UINT16(3, missing[1]);
}

void test_bitmap_find_missing_all() {
    uint8_t bitmap[128] = {0};
    uint16_t missing[50];
    uint16_t cnt = bitmap_find_missing(bitmap, 10, missing, 50);
    TEST_ASSERT_EQUAL_UINT16(10, cnt);
}

void test_bitmap_find_missing_max_limit() {
    uint8_t bitmap[128] = {0};
    // 100 chunks missing, but max_missing = 50
    uint16_t missing[50];
    uint16_t cnt = bitmap_find_missing(bitmap, 100, missing, 50);
    TEST_ASSERT_EQUAL_UINT16(50, cnt);
}

void test_bitmap_count_set() {
    uint8_t bitmap[128] = {0};
    bitmap_set(bitmap, 0);
    bitmap_set(bitmap, 50);
    bitmap_set(bitmap, 100);
    TEST_ASSERT_EQUAL_UINT16(3, bitmap_count_set(bitmap, 200));
}

// ================================================================
// Packet Structure Size Tests
// ================================================================

void test_packet_audio_size() {
    // Audio packet should be predictable size
    TEST_ASSERT_EQUAL(sizeof(LoRaAudioPacket), 39);  // type+channel+seq+flags+ttl+sender+payload
}

void test_packet_beacon_size() {
    TEST_ASSERT_EQUAL(sizeof(LoRaBeaconPacket), 36);
}

void test_packet_file_header_size() {
    TEST_ASSERT_EQUAL(sizeof(LoRaFileHeader), 35);
}

void test_packet_file_ack_size() {
    TEST_ASSERT_EQUAL(sizeof(LoRaFileAck), 107);  // 7 + 50*2
}

void test_packet_file_end_size() {
    TEST_ASSERT_EQUAL(sizeof(LoRaFileEnd), 5);
}

// ================================================================
// Main
// ================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // CRC16
    RUN_TEST(test_crc16_empty);
    RUN_TEST(test_crc16_single_zero);
    RUN_TEST(test_crc16_known_vector);
    RUN_TEST(test_crc16_deterministic);
    RUN_TEST(test_crc16_different_data_different_crc);
    RUN_TEST(test_crc16_large_buffer);

    // Bitmap
    RUN_TEST(test_bitmap_clear);
    RUN_TEST(test_bitmap_set_and_get);
    RUN_TEST(test_bitmap_various_positions);
    RUN_TEST(test_bitmap_no_duplicates);
    RUN_TEST(test_bitmap_find_missing_none);
    RUN_TEST(test_bitmap_find_missing_some);
    RUN_TEST(test_bitmap_find_missing_all);
    RUN_TEST(test_bitmap_find_missing_max_limit);
    RUN_TEST(test_bitmap_count_set);

    // Packet sizes
    RUN_TEST(test_packet_audio_size);
    RUN_TEST(test_packet_beacon_size);
    RUN_TEST(test_packet_file_header_size);
    RUN_TEST(test_packet_file_ack_size);
    RUN_TEST(test_packet_file_end_size);

    return UNITY_END();
}
