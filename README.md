# Automation, RTOS & Signal Processing Projects

[![Firmware CI](https://github.com/FaizanSSDQ/Automation-and-Signal-Processing-Projects/actions/workflows/firmware.yml/badge.svg)](https://github.com/FaizanSSDQ/Automation-and-Signal-Processing-Projects/actions/workflows/firmware.yml)
![STM32](https://img.shields.io/badge/STM32-HAL%20%7C%20FreeRTOS-03234B?logo=stmicroelectronics&logoColor=white)
![nRF52](https://img.shields.io/badge/nRF52-Zephyr%20%7C%20BLE-00A9CE?logo=nordicsemiconductor&logoColor=white)
![ESP32](https://img.shields.io/badge/ESP32-Arduino%20%7C%20FreeRTOS-E7352C?logo=espressif&logoColor=white)
![MATLAB](https://img.shields.io/badge/MATLAB-DSP-0076A8)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

Firmware and signal-processing projects: real-time data acquisition with DMA, FreeRTOS and
Zephyr task design, Bluetooth Low Energy, waveform generation, digital communication (FSK),
logic analysis, and DSP fundamentals.

Every project is documented as an engineering case study: system block diagram, pin map,
firmware architecture, timing analysis, design decisions, build instructions and a hardware
bring-up checklist. STM32 projects build from the command line and are compiled and unit-tested
in CI on every push.

## Projects

### STM32 (C, STM32 HAL, FreeRTOS)

| Project | Board | Highlights | Status |
|---|---|---|---|
| [Dual-channel ADC sampler](stm32/dual-channel-adc-dma-sampler) | NUCLEO-L152RE | TIM3-triggered scan, ping-pong circular DMA, integer statistics, CRC-framed UART DMA telemetry, Python live viewer | ✅ Complete |
| [FSK modem](stm32/fsk-modem) | NUCLEO-L152RE | 1000-baud FSK over DAC/ADC: NCO transmitter, sliding-correlator receiver, UART-style framing, BER matching theory | ✅ Complete |
| [8-channel logic analyzer](stm32/sump-logic-analyzer) | NUCLEO-F103RB | 4 MS/s timer-paced GPIO DMA, level and edge triggers with pre-trigger, SUMP protocol for PulseView, host simulator | ✅ Complete |
| [DDS waveform generator](stm32/dds-waveform-generator) | NUCLEO-F103RB | 0.01 Hz–20 kHz DDS, PWM DAC fed by DMA burst, PolyBLEP anti-aliasing, sweeps, serial control | ✅ Complete |
| [FreeRTOS task scheduling](stm32/freertos-task-scheduling) | NUCLEO-F103RB | Live priorities, preemption, starvation and time slicing; per-task CPU % from run-time stats; task-switch trace pins | ✅ Complete |
| [Periodic tasks: drift vs jitter](stm32/freertos-periodic-tasks-jitter) | NUCLEO-F103RB | vTaskDelay vs vTaskDelayUntil under interference, cycle-counter timestamps, live period histograms | ✅ Complete |
| [Priority inversion vs inheritance](stm32/freertos-priority-inversion) | NUCLEO-F103RB | Binary semaphore vs mutex in a live low/medium/high scenario, blocking time measured per cycle | ✅ Complete |

### Nordic nRF52 (Zephyr, nRF Connect SDK)

| Project | Highlights | Status |
|---|---|---|
| [nRF52 projects](nrf52) | BLE, low-power design, sensor drivers and secure OTA updates | 🔧 In preparation |

### ESP32 (Arduino core, FreeRTOS)

| Project | Highlights | Status |
|---|---|---|
| [FreeRTOS Wi-Fi robot car](esp32/freertos-wifi-robot-car) | Dual-core tasks, phone joystick page, obstacle avoidance and line following, link watchdog and fused distance sensing | ✅ Complete |
| [ESP-NOW sensor network](esp32/espnow-sensor-network) | Deep-sleep nodes with alarm wake-up, CRC-checked packets, gateway dashboard with loss and RSSI per node, ThingSpeak upload | ✅ Complete |
| [Ultrasonic and infrared radar](esp32/ultrasonic-radar) | 180° servo sweep, interrupt-timed echoes with temperature correction, object detection, live radar page in the browser | ✅ Complete |
| [ESP32-CAM streaming server](esp32/esp32cam-streaming-server) | MJPEG video on its own server and core, login with lockout, stream token, snapshots and live camera settings | ✅ Complete |
| [Wi-Fi waveform generator](esp32/wifi-waveform-generator) | DDS into both DACs through DMA, PolyBLEP anti-aliasing, sweeps, sync output, browser control with a preview of the real samples | ✅ Complete |
| [Logic analyzer](esp32/logic-analyzer) | Digital signal capture and display | 🔧 Being rebuilt |

### MATLAB (signals and systems, DSP)

| Project | Highlights | Status |
|---|---|---|
| [Signal basics](matlab/signal-basics) | Impulse, step, ramp and sinusoidal signals | 🔧 Being rebuilt |
| [Fourier series](matlab/fourier-series) | Series approximation and harmonic analysis | 🔧 Being rebuilt |
| [Fourier transform](matlab/fourier-transform) | Spectra of multiple signals | 🔧 Being rebuilt |
| [Filter design](matlab/filter-design) | Filtering and signal power | 🔧 Being rebuilt |
| [Function generator](matlab/function-generator) | Rectangular, triangular and pulse waveforms | 🔧 Being rebuilt |

### Python (vehicle modelling)

| Project | Highlights | Status |
|---|---|---|
| [Vehicle kinematic model](python/vehicle-kinematic-model) | Bicycle-model trajectory simulation | 🔧 Being rebuilt |
| [EKF vehicle state estimation](python/ekf-vehicle-state-estimation) | Error-state Kalman filter with IMU, GNSS and LiDAR | 🔧 Being rebuilt |

## Repository layout
```
stm32/    STM32CubeIDE projects (each with .ioc, Makefile, tests and docs)
nrf52/    Nordic nRF52 projects on Zephyr / nRF Connect SDK
esp32/    Arduino-core ESP32 sketches
matlab/   MATLAB signal-processing scripts
python/   Python simulations
```

The original course versions of the ESP32 projects, and the communication-system projects (NRF24, Bluetooth), live in
[Electronics-Engineering-Projects](https://github.com/FaizanSSDQ/Electronics-Engineering-Projects)
and [Embedded-Systems-and-MATLAB-Project](https://github.com/FaizanSSDQ/Embedded-Systems-and-MATLAB-Project).

---
## 👤 About the Author

**Faizan Saleem Siddiqui**, Embedded Firmware Engineer | Visiting Faculty | Research Scholar

Electronics engineer with 2+ years of experience in embedded systems and firmware development.
I build firmware for **BLE wearables** and **Bluetooth TWS audio devices** on Nordic nRF52 (Zephyr /
nRF Connect SDK), including **secure OTA updates (MCUboot, signed images)**, **low-power battery
management** and **charging-case firmware**. My background also covers STM32 and ESP32 with
FreeRTOS, real-time data acquisition, digital signal processing, industrial IoT, and Python tooling
for data, automation and machine learning.

- 🎓 MS Electronics, Quaid-i-Azam University, Islamabad (BS Electronics: 1st position, merit scholar)
- 🧑‍🏫 Visiting Faculty Member, Quaid-i-Azam University: teaching Real-Time Systems (STM32, FreeRTOS),
  Microcontroller Interfacing (ESP32) and AI/ML on Embedded Devices (TinyML)
- 🔬 Former Research Assistant, Autonomous Vehicle Engineering (COMSIP Lab, QAU)
- 🌍 Open to embedded / firmware engineering opportunities worldwide

[![LinkedIn](https://img.shields.io/badge/LinkedIn-Faizan%20Saleem%20Siddiqui-0A66C2?logo=linkedin&logoColor=white)](https://www.linkedin.com/in/faizan-saleem-siddiqui-4411bb247/)
[![GitHub](https://img.shields.io/badge/GitHub-FaizanSSDQ-181717?logo=github&logoColor=white)](https://github.com/FaizanSSDQ)
[![Email](https://img.shields.io/badge/Email-faizansaleemsdq%40yahoo.com-6001D2?logo=yahoo&logoColor=white)](mailto:faizansaleemsdq@yahoo.com)

Licensed under the [MIT License](LICENSE). Vendor code (STM32 HAL, CMSIS) keeps its own license.
