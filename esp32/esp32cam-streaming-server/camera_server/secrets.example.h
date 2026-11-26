/**
 * @file    secrets.example.h
 * @brief   Copy to secrets.h (ignored by git) and fill in. Never commit real
 *          passwords: anyone who can read the repository could log in.
 */
#ifndef SECRETS_H
#define SECRETS_H

/* Router to join. Leave WIFI_SSID empty to run as an access point instead. */
#define WIFI_SSID       ""
#define WIFI_PASSWORD   ""

/* Login for the camera page and stream (HTTP Basic authentication). */
#define CAM_USER        "admin"
#define CAM_PASSWORD    "change-me-now"

/* Password of the camera's own access point (at least 8 characters). */
#define AP_PASSWORD     "esp32cam123"

#endif /* SECRETS_H */
