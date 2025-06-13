#include "gps.h"
#include "stm32f1xx.h"
#include "timer.h"
#include "led.h"
#include "lora.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct {
    float lat;
    float lon;
} Point;
//static const Point fence[] = {
//    {21.0405035f, 105.792178f}, // Điểm 1: Tây nam
//    {21.0405035f, 105.794178f}, // Điểm 2: Đông nam
//    {21.0425035f, 105.794178f}, // Điểm 3: Đông bắc
//    {21.0425035f, 105.792178f}  // Điểm 4: Tây bắc
//};
static const Point fence[] = {
    {21.03559933f, 105.787242f},
    {21.03559933f, 105.789242f},
    {21.03759933f, 105.789242f},
    {21.03759933f, 105.787242f}
};

#define UART_BUFFER_SIZE 512
static char uart_buffer[UART_BUFFER_SIZE];
static uint8_t uart_index = 0;

static float current_lat = 0.0f;
static float current_lon = 0.0f;
static uint32_t current_timestamp = 0;

static uint32_t calculate_unix_timestamp(int year, int month, int day, int hour, int min, int sec) {
    int years = year - 1970;
    int leaps = (years + 1) / 4;
    uint32_t days = years * 365 + leaps;

    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int i = 0; i < month - 1; i++) {
        days += days_in_month[i];
    }
    if (month > 2 && year % 4 == 0) days++;
    days += day - 1;

    return days * 86400UL + hour * 3600UL + min * 60UL + sec;
}

static int validate_nmea_checksum(const char* nmea) {
    if (nmea[0] != '$') return 0;
    const char* asterisk = strchr(nmea, '*');
    if (!asterisk || strlen(asterisk) < 3) return 0;

    uint8_t calculated_checksum = 0;
    for (const char* p = nmea + 1; p < asterisk; p++) {
        calculated_checksum ^= *p;
    }

    char checksum_str[3] = {0};
    strncpy(checksum_str, asterisk + 1, 2);
    uint8_t received_checksum = (uint8_t)strtol(checksum_str, NULL, 16);

    return calculated_checksum == received_checksum;
}

void DEBUG_UART_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOA->CRH |= (GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1); // PA9: TX

    USART1->BRR = SystemCoreClock / 115200; // Baud rate 115200
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
}

void DEBUG_Print(const char* str) {
    while (*str) {
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = *str++;
    }
}

void GPS_UART_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2);
    GPIOA->CRL |= (GPIO_CRL_MODE2_1 | GPIO_CRL_CNF2_1); // PA2: TX
    GPIOA->CRL &= ~(GPIO_CRL_MODE3 | GPIO_CRL_CNF3);
    GPIOA->CRL |= GPIO_CRL_CNF3_0; // PA3: RX

    USART2->BRR = SystemCoreClock / 9600; // Baud rate 9600
    USART2->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;
    NVIC_SetPriority(USART2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_IRQHandler(void) {
    if (USART2->SR & USART_SR_RXNE) {
        char c = (char)(USART2->DR & 0xFF);
        if (uart_index < UART_BUFFER_SIZE - 1) {
            if (c == '$') {
                uart_index = 0;
                uart_buffer[uart_index++] = c;
            } else if (c == '\n' && uart_index > 0) {
                uart_buffer[uart_index] = '\0';
                if (uart_index >= 6 && uart_buffer[0] == '$') {
                    char debug_buf[512];
                    snprintf(debug_buf, sizeof(debug_buf), "NMEA: %s\r\n", uart_buffer);
                    DEBUG_Print(debug_buf);
                    if (strncmp(uart_buffer, "$GPGGA", 6) == 0 || strncmp(uart_buffer, "$GPRMC", 6) == 0) {
                        if (validate_nmea_checksum(uart_buffer)) {
                            GPS_Parse_NMEA(uart_buffer);
                        } else {
                            snprintf(debug_buf, sizeof(debug_buf), "Checksum failed: %s\r\n", uart_buffer);
                            DEBUG_Print(debug_buf);
                        }
                    } else {
                        snprintf(debug_buf, sizeof(debug_buf), "Non-GPGGA/GPRMC sentence: %s\r\n", uart_buffer);
                        DEBUG_Print(debug_buf);
                    }
                }
                uart_index = 0;
            } else if (c != '\r' && uart_index > 0) {
                uart_buffer[uart_index++] = c;
            }
        } else {
            uart_index = 0;
            char debug_buf[128];
            snprintf(debug_buf, sizeof(debug_buf), "UART buffer overflow\r\n");
            DEBUG_Print(debug_buf);
        }
    }
    if (USART2->SR & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) {
        USART2->SR;
        USART2->DR;
        uart_index = 0;
        char debug_buf[128];
        snprintf(debug_buf, sizeof(debug_buf), "UART error detected\r\n");
        DEBUG_Print(debug_buf);
    }
}

void GPS_Parse_NMEA(char* nmea) {
    char debug_buf[128];
    char* saveptr;

    snprintf(debug_buf, sizeof(debug_buf), "Parsing NMEA: %s\r\n", nmea);
    DEBUG_Print(debug_buf);

    if (strncmp(nmea, "$GPGGA", 6) == 0) {
        char* token = strtok_r(nmea, ",", &saveptr);
        int field = 0;
        char lat_str[12] = {0}, lon_str[12] = {0}, lat_dir[2] = {0}, lon_dir[2] = {0};
        int fix_quality = 0;

        while (token != NULL && field <= 6) {
            if (field == 0) { // $GPGGA
                token = strtok_r(NULL, ",", &saveptr);
                field++;
                continue;
            }
            else if (field == 2) strncpy(lat_str, token, sizeof(lat_str) - 1); // Latitude
            else if (field == 3) strncpy(lat_dir, token, sizeof(lat_dir) - 1); // N/S
            else if (field == 4) strncpy(lon_str, token, sizeof(lon_str) - 1); // Longitude
            else if (field == 5) strncpy(lon_dir, token, sizeof(lon_dir) - 1); // E/W
            else if (field == 6) fix_quality = atoi(token); // Fix quality
            token = strtok_r(NULL, ",", &saveptr);
            field++;
        }

        snprintf(debug_buf, sizeof(debug_buf), "Raw GPGGA: fix=%d, lat=%s, lon=%s, lat_dir=%s, lon_dir=%s\r\n",
                 fix_quality, lat_str, lon_str, lat_dir, lon_dir);
        DEBUG_Print(debug_buf);

        if (fix_quality == 0 || lat_str[0] == '\0' || lon_str[0] == '\0' || lat_dir[0] == '\0' || lon_dir[0] == '\0') {
            snprintf(debug_buf, sizeof(debug_buf), "Invalid GPGGA: fix=%d, lat=%s, lon=%s, lat_dir=%s, lon_dir=%s\r\n",
                     fix_quality, lat_str, lon_str, lat_dir, lon_dir);
            DEBUG_Print(debug_buf);
            return;
        }

        float lat = atof(lat_str);
        int lat_deg = (int)(lat / 100);
        float lat_min = lat - (lat_deg * 100);
        if (lat_deg >= 0 && lat_min >= 0.0f) {
            current_lat = lat_deg + lat_min / 60.0f;
            if (lat_dir[0] == 'S') current_lat = -current_lat;
        } else {
            snprintf(debug_buf, sizeof(debug_buf), "Invalid latitude: deg=%d, min=%.2f\r\n", lat_deg, lat_min);
            DEBUG_Print(debug_buf);
            return;
        }

        float lon = atof(lon_str);
        int lon_deg = (int)(lon / 100);
        float lon_min = lon - (lon_deg * 100);
        if (lon_deg >= 0 && lon_min >= 0.0f) {
            current_lon = lon_deg + lon_min / 60.0f;
            if (lon_dir[0] == 'W') current_lon = -current_lon;
        } else {
            snprintf(debug_buf, sizeof(debug_buf), "Invalid longitude: deg=%d, min=%.2f\r\n", lon_deg, lon_min);
            DEBUG_Print(debug_buf);
            return;
        }

        snprintf(debug_buf, sizeof(debug_buf), "Lat: %.6f, Lon: %.6f\r\n", current_lat, current_lon);
        DEBUG_Print(debug_buf);
    } else if (strncmp(nmea, "$GPRMC", 6) == 0) {
        char* token = strtok_r(nmea, ",", &saveptr);
        int field = 0;
        char time_str[11] = {0}, date_str[7] = {0}, status[2] = {0}, lat_str[12] = {0}, lon_str[12] = {0}, lat_dir[2] = {0}, lon_dir[2] = {0};

        while (token != NULL && field <= 9) {
            if (field == 0) { // $GPRMC
                token = strtok_r(NULL, ",", &saveptr);
                field++;
                continue;
            }
            else if (field == 1) strncpy(time_str, token, sizeof(time_str) - 1); // Time
            else if (field == 2) strncpy(status, token, sizeof(status) - 1); // Status
            else if (field == 3) strncpy(lat_str, token, sizeof(lat_str) - 1); // Latitude
            else if (field == 4) strncpy(lat_dir, token, sizeof(lat_dir) - 1); // N/S
            else if (field == 5) strncpy(lon_str, token, sizeof(lon_str) - 1); // Longitude
            else if (field == 6) strncpy(lon_dir, token, sizeof(lon_dir) - 1); // E/W
            else if (field == 9) {
                strncpy(date_str, token, sizeof(date_str) - 1); // Date
                break;
            }
            token = strtok_r(NULL, ",", &saveptr);
            field++;
        }

        snprintf(debug_buf, sizeof(debug_buf), "Raw GPRMC: status=%s, time=%s, date=%s, lat=%s, lon=%s, lat_dir=%s, lon_dir=%s\r\n",
                 status, time_str, date_str, lat_str, lon_str, lat_dir, lon_dir);
        DEBUG_Print(debug_buf);

        if (status[0] != 'A' || strlen(time_str) < 6 || strlen(date_str) != 6) {
            snprintf(debug_buf, sizeof(debug_buf), "Invalid GPRMC: status=%s, time=%s, date=%s\r\n",
                     status, time_str, date_str);
            DEBUG_Print(debug_buf);
            return;
        }

        int hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
        int min = (time_str[2] - '0') * 10 + (time_str[3] - '0');
        int sec = (time_str[4] - '0') * 10 + (time_str[5] - '0');

        int day = (date_str[0] - '0') * 10 + (date_str[1] - '0');
        int month = (date_str[2] - '0') * 10 + (date_str[3] - '0');
        int year = (date_str[4] - '0') * 10 + (date_str[5] - '0') + 2000;

        current_timestamp = calculate_unix_timestamp(year, month, day, hour, min, sec);

        // Update coordinates from GPRMC if GPGGA not available
        if (current_lat == 0.0f || current_lon == 0.0f) {
            float lat = atof(lat_str);
            int lat_deg = (int)(lat / 100);
            float lat_min = lat - (lat_deg * 100);
            if (lat_deg >= 0 && lat_min >= 0.0f) {
                current_lat = lat_deg + lat_min / 60.0f;
                if (lat_dir[0] == 'S') current_lat = -current_lat;
            }

            float lon = atof(lon_str);
            int lon_deg = (int)(lon / 100);
            float lon_min = lon - (lon_deg * 100);
            if (lon_deg >= 0 && lon_min >= 0.0f) {
                current_lon = lon_deg + lon_min / 60.0f;
                if (lon_dir[0] == 'W') current_lon = -current_lon;
            }
        }

        snprintf(debug_buf, sizeof(debug_buf), "Timestamp: %lu, Lat: %.6f, Lon: %.6f\r\n",
                 current_timestamp, current_lat, current_lon);
        DEBUG_Print(debug_buf);
    }
}

static int is_point_in_polygon(Point p, const Point* poly, int n) {
    int i, j, c = 0;
    for (i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].lat > p.lat) != (poly[j].lat > p.lat)) &&
            (p.lon < (poly[j].lon - poly[i].lon) * (p.lat - poly[i].lat) /
             (poly[j].lat - poly[i].lat) + poly[i].lon)) {
            c = !c;
        }
    }
    return c;
}

void GPS_Init(void) {
    DEBUG_UART_Init();
    GPS_UART_Init();
}

uint32_t GPS_Get_Timestamp(void) {
    return current_timestamp;
}

void GPS_Check_Geofence(uint8_t dev_id, uint32_t timestamp) {
    char debug_buf[128];
    Point p = {current_lat, current_lon};
    if (current_lat == 0.0f || current_lon == 0.0f) {
        LED_ERR_ON();
        LED_SEND_OFF();
        snprintf(debug_buf, sizeof(debug_buf), "No GPS fix: lat=%.6f, lon=%.6f\r\n", current_lat, current_lon);
        DEBUG_Print(debug_buf);
        return;
    }
    LED_ERR_OFF();
    int inside = is_point_in_polygon(p, fence, 4);

    if (inside) {
        LED_SEND_OFF();
        snprintf(debug_buf, sizeof(debug_buf), "Inside fence: lat=%.6f, lon=%.6f\r\n", current_lat, current_lon);
        DEBUG_Print(debug_buf);
    } else {
        LED_SEND_TOGGLE();
        delay_ms(100);
        lora_prepare_and_send(dev_id, timestamp, current_lat, current_lon);
        snprintf(debug_buf, sizeof(debug_buf), "Outside fence: lat=%.6f, lon=%.6f\r\n", current_lat, current_lon);
        DEBUG_Print(debug_buf);
    }
}
