#include "stm32f1xx.h"
#include "lora.h"
#include "led.h"
#include "timer.h"
#include "gps.h"

int main(void) {
    timer_init();
    LED_Init();
    SPI1_Init();
    LORA_GPIO_Init();
    GPS_Init();
    LED_POWER_ON();
    GPIOB->BSRR = GPIO_BSRR_BR10;
    lora_init();

    while (1) {
        uint32_t timestamp = GPS_Get_Timestamp();
        if (timestamp == 0) {
            timestamp = 1717587397;
        }
        GPS_Check_Geofence(0x01, timestamp);
        delay_ms(200);
    }
}
