/**
 * @file    config.h
 * @brief   Pins and settings of the Wi-Fi waveform generator (ESP32 DevKit).
 *
 * The ESP32's two 8-bit DACs are fixed to GPIO 25 (channel 0) and GPIO 26
 * (channel 1). Channel 0 carries the waveform, channel 1 a sync square wave
 * for triggering an oscilloscope.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CONFIG_H
#define CONFIG_H

#define PIN_WAVE        25      /* DAC channel 0: waveform -> RC filter -> output */
#define PIN_SYNC        26      /* DAC channel 1: sync, 0 V / 3.3 V */
#define PIN_LED         2       /* on while the output is enabled */

/* DMA: GEN_DMA_BUFFERS buffers of GEN_DMA_BUF_BYTES. With 16-bit alignment the
 * driver stores each 8-bit code in two bytes, so one buffer holds 1024 codes
 * (512 sample pairs = 5.12 ms); six buffers queue about 30 ms of output. */
#define GEN_DMA_BUFFERS     6
#define GEN_DMA_BUF_BYTES   2048

#define AP_SSID_PREFIX  "WaveGen-"      /* followed by the last 4 hex digits of the MAC */
#define AP_PASSWORD     "wavegen123"    /* at least 8 characters; change before use */

#endif /* CONFIG_H */
