"""Run the filter over a simulated drive and score it against ground truth.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass

import numpy as np

from .filter import ESEKF, NoiseSpec, State
from .rotations import qconj, qlog, qmul
from .sensors import Measurements, lidar_extrinsic
from .trajectory import Truth


@dataclass
class RunResult:
    t: np.ndarray
    p: np.ndarray
    v: np.ndarray
    q: np.ndarray
    ba: np.ndarray
    bg: np.ndarray
    sigma: np.ndarray        # n x 15, standard deviations from P
    err: np.ndarray          # n x 9: position, velocity, attitude (rad) errors, truth - estimate
    nees: np.ndarray         # normalised estimation error squared over the 9 navigation states


def run_filter(truth: Truth, meas: Measurements, *, use_gnss: bool = True, use_lidar: bool = True,
               lidar_rpy_assumed=None, estimate_biases: bool = True, noise: NoiseSpec | None = None) -> RunResult:
    spec = meas.spec
    noise = noise or NoiseSpec(acc=spec.acc_noise, gyro=spec.gyro_noise, gnss=spec.gnss_sigma, lidar=spec.lidar_sigma)
    C_li, t_li = lidar_extrinsic(lidar_rpy_assumed if lidar_rpy_assumed is not None else spec.lidar_rpy,
                                 spec.lidar_t)
    lidar_imu = (C_li @ meas.lidar_p.T).T + t_li      # LiDAR fixes moved into the IMU frame

    x0 = State(truth.p[0].copy(), truth.v[0].copy(), truth.q[0].copy(), np.zeros(3), np.zeros(3))
    P0 = np.diag([0.1] * 3 + [0.05] * 3 + [np.radians(1)] * 3 + [0.1] * 3 + [0.01] * 3) ** 2
    kf = ESEKF(x0, P0, noise, estimate_biases)

    n = len(meas.t)
    out_p, out_v, out_q = np.zeros((n, 3)), np.zeros((n, 3)), np.zeros((n, 4))
    out_ba, out_bg, out_s = np.zeros((n, 3)), np.zeros((n, 3)), np.zeros((n, 15))
    P_hist = np.zeros((n, 9, 9))
    gi = li = 0
    for k in range(n):
        if k > 0:
            dt = meas.t[k] - meas.t[k - 1]
            kf.predict(meas.imu_f[k - 1], meas.imu_w[k - 1], dt)
        tk = meas.t[k]
        while gi < len(meas.gnss_t) and meas.gnss_t[gi] <= tk + 1e-9:
            if use_gnss and abs(meas.gnss_t[gi] - tk) < 1e-9:
                kf.correct(meas.gnss_p[gi], noise.gnss)
            gi += 1
        while li < len(meas.lidar_t) and meas.lidar_t[li] <= tk + 1e-9:
            if use_lidar and abs(meas.lidar_t[li] - tk) < 1e-9:
                kf.correct(lidar_imu[li], noise.lidar)
            li += 1
        x = kf.x
        out_p[k], out_v[k], out_q[k], out_ba[k], out_bg[k] = x.p, x.v, x.q, x.ba, x.bg
        out_s[k] = np.sqrt(np.maximum(np.diag(kf.P), 0.0))
        P_hist[k] = kf.P[:9, :9]

    err = np.zeros((n, 9))
    err[:, 0:3] = truth.p - out_p
    err[:, 3:6] = truth.v - out_v
    err[:, 6:9] = np.array([qlog(qmul(qconj(qe), qt)) for qe, qt in zip(out_q, truth.q)])
    nees = np.einsum("ni,nij,nj->n", err, np.linalg.inv(P_hist), err)
    return RunResult(meas.t, out_p, out_v, out_q, out_ba, out_bg, out_s, err, nees)


def rmse(x: np.ndarray, mask=None) -> float:
    """Root-mean-square of the norm of rows of x."""
    if mask is not None:
        x = x[mask]
    return float(np.sqrt(np.mean(np.sum(x ** 2, axis=1))))
