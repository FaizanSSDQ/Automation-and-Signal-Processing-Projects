/**
 * @file    app_fsk.h
 * @brief   FSK modem application: UART terminal <-> DAC transmitter / ADC receiver.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#ifndef APP_FSK_H
#define APP_FSK_H

#ifdef __cplusplus
extern "C" {
#endif

/** Start the DAC transmitter, ADC receiver and UART terminal. */
void app_fsk_start(void);

/** Run receiver processing and terminal output. Call continuously from the main loop. */
void app_fsk_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FSK_H */
