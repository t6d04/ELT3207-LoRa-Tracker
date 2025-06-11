#ifndef __LORA_PACKET_H
#define __LORA_PACKET_H

#include <stdint.h>

// === Kích thước gói LoRa ===
#define LORA_PACKET_SIZE 14

// === Cấu trúc dữ liệu nguồn ===
typedef struct {
    uint8_t dev_id;
    uint32_t timestamp;
    float latitude;
    float longitude;
} LoRaPacket_t;

// === Hàm ===
void lora_packet_prepare(uint8_t* out_buffer, LoRaPacket_t* pkt);
uint8_t lora_packet_checksum(uint8_t* data, uint8_t len);

#endif
