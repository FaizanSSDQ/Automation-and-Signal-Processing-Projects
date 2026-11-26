/**
 * @file    app_la.h
 * @brief   Logic analyzer application: SUMP host link and capture control.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef APP_LA_H
#define APP_LA_H

/** Start the host link and the demo signals. Call once after peripheral init. */
void app_la_init(void);

/** Main-loop service: host commands, trigger scanning, data upload. */
void app_la_run(void);

#endif /* APP_LA_H */
