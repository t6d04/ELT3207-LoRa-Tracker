// lora.h - Header cho module LoRa SX1278 sử dụng STM32F103C8T6
#ifndef __LORA_H
#define __LORA_H

#include <stdint.h>

void SPI1_Init(void);
void LORA_GPIO_Init(void);
void lora_init(void);
void lora_prepare_and_send(uint8_t dev_id, uint32_t timestamp, float lat, float lon);
void lora_send_packet(uint8_t* data, uint8_t len);

#endif // __LORA_H
