/**
 * @file    demo_signals.h
 * @brief   On-board test signals for trying the analyzer with jumper wires only.
 *
 *   PA6  (D12)  TIM3_CH1   10 kHz square wave, 50 % duty
 *   PA7  (D11)  TIM3_CH2   10 kHz pulse, 25 % duty (rises together with PA6)
 *   PB6  (D10)  TIM4_CH1   1 kHz pulse, 10 % duty
 *   PA9  (D8)   USART1_TX  9600 baud 8N1, "LA demo NNNN\r\n" every 50 ms
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef DEMO_SIGNALS_H
#define DEMO_SIGNALS_H

#define DEMO_UART_PERIOD_MS 50u

/** Start the PWM outputs. */
void demo_signals_start(void);

/** Send the next UART demo message when it is due. Call from the main loop. */
void demo_signals_poll(void);

#endif /* DEMO_SIGNALS_H */
