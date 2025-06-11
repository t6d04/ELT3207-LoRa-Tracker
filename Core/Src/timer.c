#include "stm32f1xx.h"
#include "timer.h"

void timer_init(void) {
    // Sử dụng HCLK = 72MHz
    SysTick->CTRL = 0;                   // Tắt SysTick
    SysTick->LOAD = 0xFFFFFF;            // Max 24-bit (16,777,215)
    SysTick->VAL  = 0;                   // Reset bộ đếm
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    // Clock source = HCLK, Enable
}

void delay_us(uint32_t us) {
    uint32_t ticks = us * 72;  // 1us = 72 ticks @72MHz
    uint32_t start = SysTick->VAL;
    uint32_t elapsed = 0;
    uint32_t prev = start;

    while (elapsed < ticks) {
        uint32_t now = SysTick->VAL;
        uint32_t diff = (prev >= now)
                        ? (prev - now)
                        : (prev + (SysTick->LOAD + 1) - now);
        elapsed += diff;
        prev = now;
    }
}

void delay_ms(uint32_t ms) {
    while (ms--) {
        delay_us(1000);
    }
}
