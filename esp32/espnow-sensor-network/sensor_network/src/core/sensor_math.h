/**
 * @file    sensor_math.h
 * @brief   DHT22 frame decoding and BMP280 compensation (no hardware dependency).
 *
 * DHT22: the sensor answers with 40 bits; each bit is a ~50 us low pulse
 * followed by a high pulse of ~26 us (0) or ~70 us (1). Bytes: humidity high,
 * humidity low, temperature high (bit 7 = sign), temperature low, checksum.
 *
 * BMP280: raw 20-bit temperature and pressure readings are turned into degC
 * and Pa with the factory calibration (24 bytes at register 0x88) using the
 * integer formulas from the Bosch datasheet (section 3.11.3 and 8.2).
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef SENSOR_MATH_H
#define SENSOR_MATH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DHT_BIT_THRESHOLD_US 48u   /* high pulse longer than this is a 1 */

/** Bytes from 40 measured high-pulse widths. */
void dht_bits_to_bytes(const uint8_t high_us[40], uint8_t bytes[5]);

/** Check the checksum and convert; false on a checksum error. */
bool dht22_convert(const uint8_t bytes[5], int16_t *temp_c100, uint16_t *humidity_x100);

typedef struct {
    uint16_t T1;
    int16_t T2, T3;
    uint16_t P1;
    int16_t P2, P3, P4, P5, P6, P7, P8, P9;
} bmp280_calib_t;

/** Parse the 24 calibration bytes read from register 0x88. */
void bmp280_parse_calib(const uint8_t raw[24], bmp280_calib_t *c);

/** Raw 20-bit value from the three data registers (msb, lsb, xlsb). */
int32_t bmp280_raw20(uint8_t msb, uint8_t lsb, uint8_t xlsb);

/**
 * Compensate raw readings. Temperature in 0.01 degC, pressure in Pa.
 * @return false if the pressure cannot be computed (calibration invalid).
 */
bool bmp280_compensate(const bmp280_calib_t *c, int32_t adc_t, int32_t adc_p,
                       int32_t *temp_c100, uint32_t *pressure_pa);

/** Battery voltage in mV from the ADC voltage behind a divider of @p ratio_x100 / 100. */
uint16_t battery_mv(uint32_t adc_mv, uint32_t ratio_x100);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_MATH_H */
