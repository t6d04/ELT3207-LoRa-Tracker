// led.c - Debug LED support for Tracker
#include "led.h"

void LED_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    // PB0, PB1, PB10, PB11: output push-pull 2MHz
    GPIOB->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0);
    GPIOB->CRL |=  (GPIO_CRL_MODE0_1); // PB0

    GPIOB->CRL &= ~(GPIO_CRL_CNF1 | GPIO_CRL_MODE1);
    GPIOB->CRL |=  (GPIO_CRL_MODE1_1); // PB1

    GPIOB->CRH &= ~(GPIO_CRH_CNF10 | GPIO_CRH_MODE10);
    GPIOB->CRH |=  (GPIO_CRH_MODE10_1); // PB10

    GPIOB->CRH &= ~(GPIO_CRH_CNF11 | GPIO_CRH_MODE11);
    GPIOB->CRH |=  (GPIO_CRH_MODE11_1); // PB11
}

void LED_POWER_ON(void) {
    GPIOB->BSRR = GPIO_BSRR_BS0;
}

void LED_OK_ON(void) {
    GPIOB->BSRR = GPIO_BSRR_BS1;
}

void LED_ERR_ON(void) {
    GPIOB->BSRR = GPIO_BSRR_BS11;
}

void LED_SEND_TOGGLE(void) {
    GPIOB->ODR ^= (1 << 10);  // Toggle PB10
}
