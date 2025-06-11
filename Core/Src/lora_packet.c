#include "lora_packet.h"
#include <string.h>

// === Tính XOR checksum trên mảng byte ===
uint8_t lora_packet_checksum(uint8_t* data, uint8_t len) {
    uint8_t cs = 0;
    for (uint8_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

// === Đóng gói dữ liệu thành mảng 14 byte ===
void lora_packet_prepare(uint8_t* out_buffer, LoRaPacket_t* pkt) {
    uint8_t idx = 0;

    out_buffer[idx++] = pkt->dev_id;

    // timestamp (uint32_t → 4 byte, little endian)
    out_buffer[idx++] = (uint8_t)(pkt->timestamp & 0xFF);
    out_buffer[idx++] = (uint8_t)((pkt->timestamp >> 8) & 0xFF);
    out_buffer[idx++] = (uint8_t)((pkt->timestamp >> 16) & 0xFF);
    out_buffer[idx++] = (uint8_t)((pkt->timestamp >> 24) & 0xFF);

    // latitude (float → 4 byte)
    union {
        float f;
        uint8_t b[4];
    } lat;
    lat.f = pkt->latitude;
    for (int i = 0; i < 4; i++) out_buffer[idx++] = lat.b[i];

    // longitude (float → 4 byte)
    union {
        float f;
        uint8_t b[4];
    } lon;
    lon.f = pkt->longitude;
    for (int i = 0; i < 4; i++) out_buffer[idx++] = lon.b[i];

    // checksum = XOR 13 byte đầu
    out_buffer[idx++] = lora_packet_checksum(out_buffer, 13);
}
