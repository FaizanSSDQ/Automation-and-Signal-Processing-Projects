# MATLAB Projects: Wireless PHY, Wearable Signal Processing, Filters and Fourier Analysis

![MATLAB](https://img.shields.io/badge/MATLAB-no%20toolboxes-0076A8?logo=mathworks&logoColor=white)
![Octave](https://img.shields.io/badge/GNU%20Octave-compatible-0790C0?logo=octave&logoColor=white)
![CI](https://img.shields.io/badge/CI-MATLAB%20%2B%20Octave-2f855a)

Signal-processing projects written in base MATLAB, without any toolbox, so every algorithm is
visible in the code. Each project has its functions in `src/`, a test suite in `tests/`, the
script that draws its figures in `examples/`, and a README with the results and a plain-language
section for students. Continuous integration runs every test suite on MATLAB and on GNU Octave.

## Projects

| Project | What it does | Tests |
|---|---|---|
| [**Bluetooth LE GFSK modem**](ble-gfsk-modem) | The LE 1M physical layer end to end: packet with CRC-24 and whitening, GFSK modulation, channel filter and discriminator, access-address synchronisation, carrier-offset removal. 0.1 % BER at 11.6 dB Eb/N0, 0.7 dB from theory | 23 |
| [**PPG heart-rate estimator**](ppg-heart-rate-estimator) | Heart rate from wrist PPG by peaks and by a tracked spectral estimate; motion artefacts removed with an NLMS filter using the accelerometer. Running: 47.5 bpm error without it, 0.1 bpm with it | 12 |
| [**IMU orientation fusion**](imu-orientation-fusion) | Quaternions; gyro integration, complementary, Mahony and Madgwick filters against exact ground truth (Mahony 1.05° tilt RMS); free fall → impact → stillness fall detection | 16 |
| [**Digital filter designer**](digital-filter-designer) | Kaiser FIR and Butterworth IIR designed from first principles, bit-exact Q15 biquad model, CMSIS-DSP header export; shows a 16-bit 8th-order direct-form filter going unstable | 33 |
| [**Fourier analysis lab**](fourier-analysis-lab) | Fourier series checked against closed forms, Gibbs overshoot measured at 8.95 %, Parseval, FFT-based transforms, window side lobes and leakage | 27 |

## Course work (original)

The first versions of these topics, from the Signals and Systems course, are kept as they were:
[signal-basics](signal-basics), [fourier-series](fourier-series),
[fourier-transform](fourier-transform), [filter-design](filter-design) and
[function-generator](function-generator). The Fourier analysis lab and the digital filter
designer are their rebuilt, tested versions.

## Running the projects

```matlab
cd matlab/<project>
addpath tests;    run_tests        % prints the checks; errors if any fails
addpath examples; make_figures     % regenerates docs/*.png
```

Tested in CI on the current MATLAB release and on GNU Octave. In Octave the figure script briefly opens its
plot windows, because Octave's FLTK renderer can only capture visible figures.

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

Application code licensed under the [MIT License](../LICENSE).
