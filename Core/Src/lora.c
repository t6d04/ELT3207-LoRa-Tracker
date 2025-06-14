#include "lora.h"
#include "led.h"
#include "timer.h"
#include <string.h>
#include <stdio.h> // Thêm để dùng snprintf

#define LORA_NSS_LOW()   (GPIOA->BSRR = GPIO_BSRR_BR4)
#define LORA_NSS_HIGH()  (GPIOA->BSRR = GPIO_BSRR_BS4)
#define LORA_RST_LOW()   (GPIOA->BSRR = GPIO_BSRR_BR1)   // PA1: RST
#define LORA_RST_HIGH()  (GPIOA->BSRR = GPIO_BSRR_BS1)

void SPI1_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_SPI1EN;

    GPIOA->CRL &= ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5);
    GPIOA->CRL |=  (GPIO_CRL_MODE5_1 | GPIO_CRL_MODE5_0 | GPIO_CRL_CNF5_1); // PA5: SCK

    GPIOA->CRL &= ~(GPIO_CRL_CNF7 | GPIO_CRL_MODE7);
    GPIOA->CRL |=  (GPIO_CRL_MODE7_1 | GPIO_CRL_MODE7_0 | GPIO_CRL_CNF7_1); // PA7: MOSI

    GPIOA->CRL &= ~(GPIO_CRL_CNF6 | GPIO_CRL_MODE6);
    GPIOA->CRL |= GPIO_CRL_CNF6_0; // PA6: MISO

    GPIOA->CRL &= ~(GPIO_CRL_CNF4 | GPIO_CRL_MODE4);
    GPIOA->CRL |= (GPIO_CRL_MODE4_1 | GPIO_CRL_MODE4_0); // PA4: NSS

    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM | SPI_CR1_BR_1;
    SPI1->CR1 |= SPI_CR1_SPE;
}

void LORA_GPIO_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOA->CRL |= (GPIO_CRL_MODE1_1 | GPIO_CRL_MODE1_0); // PA1: RST

    GPIOA->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);
    GPIOA->CRL |= (GPIO_CRL_MODE4_1 | GPIO_CRL_MODE4_0); // PA4: NSS

    LORA_NSS_HIGH();
    LORA_RST_HIGH();
}

static void spi1_write(uint8_t data) {
    while (!(SPI1->SR & SPI_SR_TXE));
    *(volatile uint8_t*)&SPI1->DR = data;
    while (!(SPI1->SR & SPI_SR_RXNE));
    (void)SPI1->DR;
}

static uint8_t spi1_transfer(uint8_t data) {
    while (!(SPI1->SR & SPI_SR_TXE));
    *(volatile uint8_t*)&SPI1->DR = data;
    while (!(SPI1->SR & SPI_SR_RXNE));
    return (uint8_t)SPI1->DR;
}

static void lora_write_reg(uint8_t addr, uint8_t value) {
    LORA_NSS_LOW();
    spi1_write(addr | 0x80);
    spi1_write(value);
    LORA_NSS_HIGH();
}

static uint8_t lora_read_reg(uint8_t addr) {
    LORA_NSS_LOW();
    spi1_write(addr & 0x7F);
    uint8_t val = spi1_transfer(0x00);
    LORA_NSS_HIGH();
    return val;
}

void lora_init(void) {
    LORA_RST_LOW();
    for (volatile int i = 0; i < 100000; i++);
    LORA_RST_HIGH();
    for (volatile int i = 0; i < 100000; i++);

    uint8_t version = lora_read_reg(0x42);
    if (version == 0x12) {
        LED_OK_ON();
    } else {
        LED_ERR_ON();
        return; // Không treo
    }

    lora_write_reg(0x01, 0x80); // Sleep + LoRa
    lora_write_reg(0x01, 0x81); // Standby + LoRa

    lora_write_reg(0x06, 0x6C); // 433 MHz
    lora_write_reg(0x07, 0x80);
    lora_write_reg(0x08, 0x00);

    lora_write_reg(0x20, 0x00); // Preamble
    lora_write_reg(0x21, 0x08);

    lora_write_reg(0x39, 0x12); // Sync word

    lora_write_reg(0x1D, 0x72); // BW 125 kHz, CR 4/5
    lora_write_reg(0x1E, 0xC4); // SF12
    lora_write_reg(0x26, 0x04); // Low data rate

    lora_write_reg(0x0E, 0x00); // FIFO TX base
}

void lora_prepare_and_send(uint8_t dev_id, uint32_t timestamp, float lat, float lon) {
    uint8_t packet[14];
    union {
        float f;
        uint8_t b[4];
    } lat_u, lon_u;

    packet[0] = dev_id;
    packet[1] = (uint8_t)(timestamp & 0xFF);
    packet[2] = (uint8_t)((timestamp >> 8) & 0xFF);
    packet[3] = (uint8_t)((timestamp >> 16) & 0xFF);
    packet[4] = (uint8_t)((timestamp >> 24) & 0xFF);

    lat_u.f = lat;
    for (int i = 0; i < 4; i++) packet[5 + i] = lat_u.b[i];

    lon_u.f = lon;
    for (int i = 0; i < 4; i++) packet[9 + i] = lon_u.b[i];

    uint8_t crc = 0;
    for (int i = 0; i < 13; i++) crc ^= packet[i];
    packet[13] = crc;

    // Debug gói tin qua Serial
    char debug_buf[128];
    snprintf(debug_buf, sizeof(debug_buf), "Packet: ");
    DEBUG_Print(debug_buf);
    for (int i = 0; i < 14; i++) {
        snprintf(debug_buf, sizeof(debug_buf), "0x%02X ", packet[i]);
        DEBUG_Print(debug_buf);
    }
    snprintf(debug_buf, sizeof(debug_buf), "\r\nSending: DevID=%d, Timestamp=%u, Lat=%.6f, Lon=%.6f\r\n",
             dev_id, timestamp, lat, lon);
    DEBUG_Print(debug_buf);

    lora_send_packet(packet, 14);
}

void lora_send_packet(uint8_t* data, uint8_t len) {
    lora_write_reg(0x01, 0x81);
    lora_write_reg(0x0D, 0x00);
    lora_write_reg(0x0E, 0x00);

    LORA_NSS_LOW();
    spi1_write(0x00 | 0x80);
    for (uint8_t i = 0; i < len; i++) {
        spi1_write(data[i]);
    }
    LORA_NSS_HIGH();

    lora_write_reg(0x22, len);
    lora_write_reg(0x01, 0x83);

    uint32_t timeout = 100000;
    while ((lora_read_reg(0x12) & 0x08) == 0 && timeout--);
    if (timeout == 0) {
        LED_ERR_ON();
        char debug_buf[64];
        snprintf(debug_buf, sizeof(debug_buf), "Tx Timeout, IRQ Flags: 0x%02X\r\n", lora_read_reg(0x12));
        DEBUG_Print(debug_buf);
        return;
    }
    lora_write_reg(0x12, 0x08);
    LED_SEND_TOGGLE();
    lora_write_reg(0x01, 0x80);
    char debug_buf[64];
    snprintf(debug_buf, sizeof(debug_buf), "Packet sent successfully\r\n");
    DEBUG_Print(debug_buf);
}
