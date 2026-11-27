"""Sensor models: IMU with biases and noise, GNSS, and LiDAR in its own frame.

IMU (200 Hz): specific force and angular rate in the body frame, each with a
constant bias and white noise.

GNSS (5 Hz): position with metre-level noise, and an optional outage
(tunnel, urban canyon) during which no fixes arrive.

LiDAR (10 Hz): positions from a scan-matching system, expressed in the LiDAR
frame. The LiDAR is mounted with an extrinsic calibration (rotation C_li,
translation t_li) such that  p_imu = C_li p_lidar + t_li . The filter must
transform the measurements with its own knowledge of that calibration; a wrong
calibration biases every LiDAR fix.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass, field

import numpy as np

from .rotations import euler_to_q, qmat
from .trajectory import Truth


@dataclass(frozen=True)
class SensorSpec:
    acc_bias: tuple = (0.06, -0.04, 0.05)          # m/s^2
    gyro_bias: tuple = (0.004, -0.003, 0.002)      # rad/s (about 0.2 deg/s)
    acc_noise: float = 0.05                        # m/s^2 per sample
    gyro_noise: float = 0.002                      # rad/s per sample
    gnss_rate: float = 5.0                         # Hz
    gnss_sigma: float = 1.0                        # m
    gnss_outage: tuple | None = None               # (start, stop) seconds
    lidar_rate: float = 10.0                       # Hz
    lidar_sigma: float = 0.3                       # m
    lidar_outage: tuple | None = None
    lidar_rpy: tuple = (0.05, 0.05, 0.10)          # rad, true mounting rotation
    lidar_t: tuple = (0.5, 0.1, 0.5)               # m, true mounting offset


@dataclass
class Measurements:
    t: np.ndarray
    imu_f: np.ndarray
    imu_w: np.ndarray
    gnss_t: np.ndarray
    gnss_p: np.ndarray
    lidar_t: np.ndarray
    lidar_p: np.ndarray            # in the LiDAR frame
    spec: SensorSpec = field(default_factory=SensorSpec)


def lidar_extrinsic(rpy, t) -> tuple[np.ndarray, np.ndarray]:
    return qmat(euler_to_q(*rpy)), np.asarray(t, float)


def _stamps(t: np.ndarray, rate: float, outage) -> np.ndarray:
    step = round(1.0 / (t[1] - t[0]) / rate)           # IMU samples between two fixes
    idx = np.arange(step, len(t), step)
    if outage is not None:
        idx = idx[(t[idx] < outage[0]) | (t[idx] >= outage[1])]
    return idx


def simulate_sensors(truth: Truth, spec: SensorSpec | None = None, seed: int = 0) -> Measurements:
    spec = spec or SensorSpec()
    rng = np.random.default_rng(seed)
    n = len(truth.t)
    imu_f = truth.f_body + np.array(spec.acc_bias) + spec.acc_noise * rng.standard_normal((n, 3))
    imu_w = truth.w_body + np.array(spec.gyro_bias) + spec.gyro_noise * rng.standard_normal((n, 3))

    gi = _stamps(truth.t, spec.gnss_rate, spec.gnss_outage)
    gnss_p = truth.p[gi] + spec.gnss_sigma * rng.standard_normal((len(gi), 3))

    li = _stamps(truth.t, spec.lidar_rate, spec.lidar_outage)
    C_li, t_li = lidar_extrinsic(spec.lidar_rpy, spec.lidar_t)
    p_noisy = truth.p[li] + spec.lidar_sigma * rng.standard_normal((len(li), 3))
    lidar_p = (C_li.T @ (p_noisy - t_li).T).T          # what the LiDAR pipeline reports

    return Measurements(truth.t, imu_f, imu_w, truth.t[gi], gnss_p, truth.t[li], lidar_p, spec)
