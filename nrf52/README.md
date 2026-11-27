# Nordic nRF52 Projects (Zephyr / nRF Connect SDK)

Firmware for Nordic nRF52-series SoCs built on the **Zephyr RTOS** with the **nRF Connect SDK**:
Bluetooth Low Energy, low-power design, sensor drivers and over-the-air firmware updates.

Each project will follow the same structure as the rest of this repository:

```
nrf52/<project>/
├── src/              application source (C)
├── boards/           devicetree overlays for the target board
├── prj.conf          Kconfig options
├── CMakeLists.txt    Zephyr build definition
├── tests/            unit tests (Ztest / host)
├── docs/             diagrams and figures
└── README.md         architecture, design decisions, build & flash, bring-up checklist
```

## Projects

Two health band projects are shown here as showcases. Their full folder structure is
published, but every source file shows only its first and last two lines; the complete code
is available on request. They also appear in
[Health-BLE-Wearable-Devices](https://github.com/FaizanSSDQ/Health-BLE-Wearable-Devices).

| Project | Highlights | Status |
|---|---|---|
| [MotionCore: IMU for HealthBand](motioncore-imu-healthband) | Wrist-motion engine: BMI270 sampled at up to 1600 Hz through the sensor FIFO with exact sensor timestamps and live loss counters; desktop monitor with 3D orientation, Mahony fusion, shock and free-fall detection and calibration | 🔒 Showcase · code on request |
| [SkinSense: Optical Proximity and Wear Detection](skinsense-optical-wear-detection) | Infrared proximity sensing for on-wrist detection and skin-contact quality; BLE GATT control and streaming, hot-plug recovery, distance calibration and a characterisation studio | 🔒 Showcase · code on request |
| *More coming* | Power optimisation and secure OTA on the nRF52 | 🔧 In preparation |

## Building

```bash
# nRF Connect SDK installed via nRF Connect for Desktop or nrfutil
west build -b nrf52840dk/nrf52840 nrf52/<project>
west flash
```

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

Licensed under the [MIT License](../LICENSE).
