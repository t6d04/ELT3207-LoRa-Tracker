// led.h - Debug LED support for Tracker
#ifndef __LED_H
#define __LED_H

#include "stm32f1xx.h"

// LED GPIO mappings
// PB0: POWER, PB1: OK, PB10: SEND, PB11: ERR
#define LED_POWER_PIN    GPIO_BSRR_BS0   // PB0
#define LED_OK_PIN       GPIO_BSRR_BS1   // PB1
#define LED_SEND_PIN     GPIO_BSRR_BS10  // PB10
#define LED_ERR_PIN      GPIO_BSRR_BS11  // PB11

// Function declarations
void LED_Init(void);
void LED_POWER_ON(void);
void LED_POWER_OFF(void);
void LED_OK_ON(void);
void LED_OK_OFF(void);
void LED_ERR_ON(void);
void LED_ERR_OFF(void);
void LED_SEND_ON(void);
void LED_SEND_OFF(void);
void LED_SEND_TOGGLE(void);

#endif // __LED_H
