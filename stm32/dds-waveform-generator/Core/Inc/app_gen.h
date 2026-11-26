/**
 * @file    app_gen.h
 * @brief   Waveform generator application: PWM/DMA output, command line, button.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef APP_GEN_H
#define APP_GEN_H

/** Start the output and the command line. Call once after peripheral init. */
void app_gen_init(void);

/** Main-loop service: commands, button, status. */
void app_gen_run(void);

#endif /* APP_GEN_H */
