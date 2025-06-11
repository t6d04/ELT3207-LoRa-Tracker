#ifndef __TIMER_H
#define __TIMER_H

#include <stdint.h>

void timer_init(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

#endif
