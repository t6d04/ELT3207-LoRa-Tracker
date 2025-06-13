#ifndef GPS_H
#define GPS_H

#include <stdint.h>

void GPS_Init(void);
void GPS_UART_Init(void);
void GPS_Parse_NMEA(char* nmea);
void GPS_Check_Geofence(uint8_t dev_id, uint32_t timestamp);
uint32_t GPS_Get_Timestamp(void);

#endif
