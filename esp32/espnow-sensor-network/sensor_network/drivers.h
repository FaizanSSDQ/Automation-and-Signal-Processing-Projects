/**
 * @file    drivers.h
 * @brief   DHT22 (single-wire) and BMP280 (I2C) drivers for the sensor node.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef DRIVERS_H
#define DRIVERS_H

#include <stdint.h>

/** Read temperature (0.01 degC) and humidity (0.01 %). Takes ~5 ms. */
bool dht22_read(int pin, int16_t *temp_c100, uint16_t *humidity_x100);

/** Check the chip ID and load the calibration. */
bool bmp280_begin(int sda, int scl, uint8_t addr);

/** One forced-mode measurement: temperature (0.01 degC) and pressure (Pa). */
bool bmp280_measure(int32_t *temp_c100, uint32_t *pressure_pa);

#endif /* DRIVERS_H */
