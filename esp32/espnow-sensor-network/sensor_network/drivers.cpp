/**
 * @file    drivers.cpp
 * @brief   DHT22 (single-wire) and BMP280 (I2C) drivers for the sensor node.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <Arduino.h>
#include <Wire.h>

#include "drivers.h"
#include "src/core/sensor_math.h"

/* ---- DHT22 ------------------------------------------------------------------------------ */

static portMUX_TYPE dht_mux = portMUX_INITIALIZER_UNLOCKED;

/* Wait until the pin reads @p level; returns the time waited in us, or -1 on timeout. */
static int32_t wait_for(int pin, int level, uint32_t timeout_us)
{
    const uint32_t start = micros();
    while (digitalRead(pin) != level) {
        if (micros() - start > timeout_us) {
            return -1;
        }
    }
    return (int32_t)(micros() - start);
}

bool dht22_read(int pin, int16_t *temp_c100, uint16_t *humidity_x100)
{
    uint8_t high_us[40];
    bool ok = true;

    pinMode(pin, OUTPUT);              /* start signal: >= 1 ms low */
    digitalWrite(pin, LOW);
    delayMicroseconds(1100);
    pinMode(pin, INPUT_PULLUP);

    /* The 40 bits take ~4 ms; an interrupt in between would corrupt the timing. */
    portENTER_CRITICAL(&dht_mux);
    if (wait_for(pin, LOW, 100) < 0 || wait_for(pin, HIGH, 100) < 0 || wait_for(pin, LOW, 100) < 0) {
        ok = false;                    /* no 80 us low / 80 us high response */
    }
    for (int i = 0; ok && i < 40; i++) {
        if (wait_for(pin, HIGH, 80) < 0) {           /* ~50 us low before each bit */
            ok = false;
            break;
        }
        const int32_t w = wait_for(pin, LOW, 100);   /* high width: ~26 us = 0, ~70 us = 1 */
        if (w < 0) {
            ok = false;
            break;
        }
        high_us[i] = (uint8_t)w;
    }
    portEXIT_CRITICAL(&dht_mux);

    if (!ok) {
        return false;
    }
    uint8_t bytes[5];
    dht_bits_to_bytes(high_us, bytes);
    return dht22_convert(bytes, temp_c100, humidity_x100);
}

/* ---- BMP280 ----------------------------------------------------------------------------- */

#define BMP280_REG_CALIB   0x88
#define BMP280_REG_ID      0xD0
#define BMP280_REG_STATUS  0xF3
#define BMP280_REG_CTRL    0xF4
#define BMP280_REG_CONFIG  0xF5
#define BMP280_REG_DATA    0xF7
#define BMP280_CHIP_ID     0x58

static uint8_t bmp_addr;
static bmp280_calib_t bmp_calib;

static bool bmp_read(uint8_t reg, uint8_t *buf, size_t n)
{
    Wire.beginTransmission(bmp_addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom(bmp_addr, (uint8_t)n) != n) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        buf[i] = (uint8_t)Wire.read();
    }
    return true;
}

static bool bmp_write(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(bmp_addr);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool bmp280_begin(int sda, int scl, uint8_t addr)
{
    bmp_addr = addr;
    Wire.begin(sda, scl, 400000);
    uint8_t id = 0;
    uint8_t raw[24];
    if (!bmp_read(BMP280_REG_ID, &id, 1) || id != BMP280_CHIP_ID || !bmp_read(BMP280_REG_CALIB, raw, 24)) {
        return false;
    }
    bmp280_parse_calib(raw, &bmp_calib);
    return bmp_write(BMP280_REG_CONFIG, 0x00);     /* no IIR filter: single reports */
}

bool bmp280_measure(int32_t *temp_c100, uint32_t *pressure_pa)
{
    /* Forced mode: temperature x1, pressure x4 oversampling, then back to sleep. */
    if (!bmp_write(BMP280_REG_CTRL, (1u << 5) | (3u << 2) | 1u)) {
        return false;
    }
    for (int i = 0; i < 20; i++) {                  /* ~14 ms conversion */
        delay(2);
        uint8_t status;
        if (bmp_read(BMP280_REG_STATUS, &status, 1) && !(status & 0x08)) {
            uint8_t d[6];
            if (!bmp_read(BMP280_REG_DATA, d, 6)) {
                return false;
            }
            return bmp280_compensate(&bmp_calib, bmp280_raw20(d[3], d[4], d[5]), bmp280_raw20(d[0], d[1], d[2]),
                                     temp_c100, pressure_pa);
        }
    }
    return false;
}
