#include "stm32f1xx.h"
#include "lora.h"
#include "timer.h"
#include "led.h"

#define LORA_NSS_LOW()   (GPIOA->BSRR = GPIO_BSRR_BR4)   // Bắt đầu giao tiếp
#define LORA_NSS_HIGH()  (GPIOA->BSRR = GPIO_BSRR_BS4)   // Kết thúc giao tiếp
#define LORA_RST_LOW()   (GPIOA->BSRR = GPIO_BSRR_BR2)
#define LORA_RST_HIGH()  (GPIOA->BSRR = GPIO_BSRR_BS2)

void SPI1_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_SPI1EN;

    GPIOA->CRL &= ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5);
    GPIOA->CRL |=  (GPIO_CRL_MODE5_1 | GPIO_CRL_MODE5_0 | GPIO_CRL_CNF5_1);

    GPIOA->CRL &= ~(GPIO_CRL_CNF7 | GPIO_CRL_MODE7);
    GPIOA->CRL |=  (GPIO_CRL_MODE7_1 | GPIO_CRL_MODE7_0 | GPIO_CRL_CNF7_1);

    GPIOA->CRL &= ~(GPIO_CRL_CNF6 | GPIO_CRL_MODE6);
    GPIOA->CRL |= GPIO_CRL_CNF6_0;

    GPIOA->CRL &= ~(GPIO_CRL_CNF4 | GPIO_CRL_MODE4);
    GPIOA->CRL |= (GPIO_CRL_MODE4_1 | GPIO_CRL_MODE4_0);

    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM | SPI_CR1_BR_1;
    SPI1->CR1 |= SPI_CR1_SPE;
}

void LORA_GPIO_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2);
    GPIOA->CRL |= (GPIO_CRL_MODE2_1 | GPIO_CRL_MODE2_0);

    GPIOA->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);
    GPIOA->CRL |= (GPIO_CRL_MODE4_1 | GPIO_CRL_MODE4_0);

    GPIOA->BSRR = GPIO_BSRR_BS4;
    GPIOA->BSRR = GPIO_BSRR_BS2;
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
        while (1);
    }

    lora_write_reg(0x01, 0x80);
    lora_write_reg(0x01, 0x81);

    lora_write_reg(0x06, 0x6C);
    lora_write_reg(0x07, 0x80);
    lora_write_reg(0x08, 0x00);

    lora_write_reg(0x20, 0x00);
    lora_write_reg(0x21, 0x08);

    lora_write_reg(0x39, 0x12);

    lora_write_reg(0x1D, 0x72);
    lora_write_reg(0x1E, 0xC4);
    lora_write_reg(0x26, 0x04);

    lora_write_reg(0x0E, 0x00);
    LED_POWER_ON();
}

void lora_prepare_and_send(uint8_t dev_id, uint32_t timestamp, float lat, float lon) {
    uint8_t* plat = (uint8_t*)&lat;
    uint8_t* plon = (uint8_t*)&lon;
    uint8_t packet[14];

    packet[0] = dev_id;
    packet[1] = (timestamp >> 0) & 0xFF;
    packet[2] = (timestamp >> 8) & 0xFF;
    packet[3] = (timestamp >> 16) & 0xFF;
    packet[4] = (timestamp >> 24) & 0xFF;

    for (int i = 0; i < 4; i++) packet[5 + i] = plat[i];
    for (int i = 0; i < 4; i++) packet[9 + i] = plon[i];

    uint8_t crc = 0;
    for (int i = 0; i < 13; i++) crc ^= packet[i];
    packet[13] = crc;

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
        return;
    }
    lora_write_reg(0x12, 0x08);
    LED_SEND_TOGGLE();
    lora_write_reg(0x01, 0x80);
}
