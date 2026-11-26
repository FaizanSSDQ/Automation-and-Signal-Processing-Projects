/**
 * @file    sensor_math.c
 * @brief   DHT22 frame decoding and BMP280 compensation.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "sensor_math.h"

void dht_bits_to_bytes(const uint8_t high_us[40], uint8_t bytes[5])
{
    for (int i = 0; i < 5; i++) {
        bytes[i] = 0;
    }
    for (int i = 0; i < 40; i++) {
        bytes[i / 8] = (uint8_t)(bytes[i / 8] << 1);
        if (high_us[i] > DHT_BIT_THRESHOLD_US) {
            bytes[i / 8] |= 1u;
        }
    }
}

bool dht22_convert(const uint8_t b[5], int16_t *temp_c100, uint16_t *humidity_x100)
{
    const uint8_t sum = (uint8_t)(b[0] + b[1] + b[2] + b[3]);
    if (sum != b[4]) {
        return false;
    }
    const uint16_t rh_x10 = (uint16_t)((b[0] << 8) | b[1]);
    int16_t t_x10 = (int16_t)(((b[2] & 0x7F) << 8) | b[3]);
    if (b[2] & 0x80) {
        t_x10 = (int16_t)-t_x10;
    }
    if (rh_x10 > 1000) {
        return false;   /* above 100 %: corrupted frame */
    }
    *humidity_x100 = (uint16_t)(rh_x10 * 10u);
    *temp_c100 = (int16_t)(t_x10 * 10);
    return true;
}

static uint16_t u16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

void bmp280_parse_calib(const uint8_t raw[24], bmp280_calib_t *c)
{
    c->T1 = u16le(&raw[0]);
    c->T2 = (int16_t)u16le(&raw[2]);
    c->T3 = (int16_t)u16le(&raw[4]);
    c->P1 = u16le(&raw[6]);
    c->P2 = (int16_t)u16le(&raw[8]);
    c->P3 = (int16_t)u16le(&raw[10]);
    c->P4 = (int16_t)u16le(&raw[12]);
    c->P5 = (int16_t)u16le(&raw[14]);
    c->P6 = (int16_t)u16le(&raw[16]);
    c->P7 = (int16_t)u16le(&raw[18]);
    c->P8 = (int16_t)u16le(&raw[20]);
    c->P9 = (int16_t)u16le(&raw[22]);
}

int32_t bmp280_raw20(uint8_t msb, uint8_t lsb, uint8_t xlsb)
{
    return (int32_t)(((uint32_t)msb << 12) | ((uint32_t)lsb << 4) | ((uint32_t)xlsb >> 4));
}

bool bmp280_compensate(const bmp280_calib_t *c, int32_t adc_t, int32_t adc_p,
                       int32_t *temp_c100, uint32_t *pressure_pa)
{
    /* Temperature: datasheet bmp280_compensate_T_int32 */
    const int32_t var1t = ((((adc_t >> 3) - ((int32_t)c->T1 << 1))) * ((int32_t)c->T2)) >> 11;
    const int32_t var2t = (((((adc_t >> 4) - ((int32_t)c->T1)) * ((adc_t >> 4) - ((int32_t)c->T1))) >> 12) *
                           ((int32_t)c->T3)) >> 14;
    const int32_t t_fine = var1t + var2t;
    *temp_c100 = (t_fine * 5 + 128) >> 8;

    /* Pressure: datasheet bmp280_compensate_P_int64, result in Q24.8 Pa. The
     * reference code left-shifts signed values, which is undefined in C for
     * negative numbers; the same scaling is written as multiplications. */
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)c->P6;
    var2 = var2 + var1 * (int64_t)c->P5 * 131072;               /* << 17 */
    var2 = var2 + (int64_t)c->P4 * 34359738368LL;              /* << 35 */
    var1 = ((var1 * var1 * (int64_t)c->P3) >> 8) + var1 * (int64_t)c->P2 * 4096;  /* << 12 */
    var1 = ((((int64_t)1 << 47) + var1) * (int64_t)c->P1) >> 33;
    if (var1 == 0) {
        return false;
    }
    int64_t p = 1048576 - adc_p;
    p = ((p * 2147483648LL - var2) * 3125) / var1;            /* << 31 */
    var1 = (((int64_t)c->P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)c->P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (int64_t)c->P7 * 16;       /* << 4 */
    *pressure_pa = (uint32_t)((p + 128) >> 8);   /* Q24.8 -> Pa, rounded */
    return true;
}

uint16_t battery_mv(uint32_t adc_mv, uint32_t ratio_x100)
{
    const uint32_t mv = (adc_mv * ratio_x100 + 50u) / 100u;
    return (uint16_t)(mv > 65535u ? 65535u : mv);
}
