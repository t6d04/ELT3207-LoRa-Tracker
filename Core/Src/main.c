// main.c - Test truyền dữ liệu LoRa với debug bằng LED
#include "stm32f1xx.h"
#include "lora.h"
#include "led.h"
#include "timer.h"

int main(void) {
	timer_init();
    LED_Init();           // Khởi tạo GPIO LED
    SPI1_Init();          // SPI cho LoRa
    LORA_GPIO_Init();     // GPIO cho NSS, RST

    LED_POWER_ON();       // Báo thiết bị đã bật nguồn

    lora_init();          // Khởi tạo LoRa (LED_OK / LED_ERR do lora.c xử lý)

    while (1) {
        uint32_t timestamp = 1717587397;
        float lat = 10.762622f;
        float lon = 106.660172f;

        lora_prepare_and_send(0x01, timestamp, lat, lon);
        delay_ms(200);
    }
}
