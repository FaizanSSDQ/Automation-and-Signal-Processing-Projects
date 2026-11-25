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
| FSK modem | NUCLEO | Tone generation and Goertzel-based detection | 🔧 In progress |
| [Logic analyzer](stm32/logic-analyzer) | NUCLEO-F103RB | Timer-sampled digital capture | 🔧 Being rebuilt |
| [Function generator](stm32/function-generator) | NUCLEO-F103RB | Lookup-table waveform synthesis | 🔧 Being rebuilt |
| [FreeRTOS: task creation](stm32/freertos-task-creation) | NUCLEO-F103RB | Tasks, priorities, scheduling | 🔧 Being rebuilt |
| [FreeRTOS: task timing](stm32/freertos-task-timing) | NUCLEO-F103RB | Periodic tasks and timing control | 🔧 Being rebuilt |
| [FreeRTOS: mutex & semaphore](stm32/freertos-mutex-semaphore) | NUCLEO-F103RB | Resource sharing and synchronisation | 🔧 Being rebuilt |

### Nordic nRF52 (Zephyr, nRF Connect SDK)

| Project | Highlights | Status |
|---|---|---|
| [nRF52 projects](nrf52) | BLE, low-power design, sensor drivers and secure OTA updates | 🔧 In preparation |

### ESP32 (Arduino core, FreeRTOS)

| Project | Highlights | Status |
|---|---|---|
| [FreeRTOS robot controller](esp32/freertos-robot-controller) | Multi-task motor control for a mobile robot | 🔧 Being rebuilt |
| [Function generator](esp32/function-generator) | Sine, square, triangle and sawtooth waveforms | 🔧 Being rebuilt |
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

Communication-system and sensor projects (NRF24, Bluetooth, radar, ESP32-CAM, IoT nodes) live in
[Electronics-Engineering-Projects](https://github.com/FaizanSSDQ/Electronics-Engineering-Projects)
and [Embedded-Systems-and-MATLAB-Project](https://github.com/FaizanSSDQ/Embedded-Systems-and-MATLAB-Project).

---
## 👤 About the Author

**Faizan Saleem Siddiqui**, Embedded Firmware Engineer | Visiting Faculty | Research Scholar

Electronics engineer with 2+ years of experience in embedded systems and firmware development.
I build production firmware for **BLE wearables and biosignal devices** on Nordic nRF52 (Zephyr / nRF
Connect SDK), including **secure OTA updates (MCUboot, signed images)**, **low-power battery
management** and **true-wireless Bluetooth audio**. My background also covers STM32 and ESP32 with
FreeRTOS, real-time data acquisition, digital signal processing, industrial IoT, and Python tooling
for data, automation and machine learning.

- 🎓 MS Electronics, Quaid-i-Azam University, Islamabad (BS Electronics: 1st position, merit scholar)
- 🧑‍🏫 Visiting Faculty Member, Quaid-i-Azam University: teaching Real-Time Systems (STM32, FreeRTOS),
  Microcontroller Interfacing (ESP32) and AI/ML on Embedded Devices (TinyML)
- 🔬 Former Research Assistant, Autonomous Vehicle Engineering (COMSIP Lab, QAU)
- 🌍 Open to embedded / firmware engineering opportunities worldwide

[![LinkedIn](https://img.shields.io/badge/LinkedIn-Faizan%20Saleem%20Siddiqui-0A66C2?logo=linkedin&logoColor=white)](https://www.linkedin.com/in/faizan-saleem-siddiqui-4411bb247/)
[![GitHub](https://img.shields.io/badge/GitHub-FaizanSSDQ-181717?logo=github&logoColor=white)](https://github.com/FaizanSSDQ)
[![Email](https://img.shields.io/badge/Email-faizansaleemsdq%40gmail.com-D14836?logo=gmail&logoColor=white)](mailto:faizansaleemsdq@gmail.com)

Licensed under the [MIT License](LICENSE). Vendor code (STM32 HAL, CMSIS) keeps its own license.
