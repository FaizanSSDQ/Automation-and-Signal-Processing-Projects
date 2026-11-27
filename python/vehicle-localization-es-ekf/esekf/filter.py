"""15-state error-state extended Kalman filter (ES-EKF) for IMU + GNSS + LiDAR.

Nominal state (non-linear, integrated from the IMU):
    p (position), v (velocity), q (orientation, body -> nav),
    b_a (accelerometer bias), b_g (gyro bias)
Error state (linear, carried by the covariance P, 15 x 15):
    dx = [dp, dv, dtheta, db_a, db_g],  q_true = q (x) exp(dtheta)

Prediction, every IMU sample (dt = 5 ms):
    a_nav = R (f - b_a) + g
    p <- p + v dt + a_nav dt^2 / 2,   v <- v + a_nav dt,   q <- q (x) exp((w - b_g) dt)
    P <- F P F' + Q,   with the error dynamics
        dp'     = dv
        dv'     = -R [f - b_a]x dtheta - R db_a
        dtheta' = -[w - b_g]x dtheta - db_g
        db_a', db_g' = random walk
Correction, for each position fix y (GNSS, or LiDAR moved into the IMU frame):
    H = [I 0 0 0 0],  K = P H' (H P H' + R)^-1,  dx = K (y - p)
    inject dx into the nominal state, P <- (I - K H) P (I - K H)' + K R K' (Joseph form)

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass

import numpy as np

from .rotations import qexp, qmat, qmul, qnormalize, skew
from .trajectory import GRAVITY


@dataclass(frozen=True)
class NoiseSpec:
    acc: float = 0.05            # m/s^2 per sample (accelerometer white noise)
    gyro: float = 0.002          # rad/s per sample
    acc_bias_walk: float = 1e-4  # m/s^2 per sqrt(s)
    gyro_bias_walk: float = 1e-5  # rad/s per sqrt(s)
    gnss: float = 1.0            # m
    lidar: float = 0.3           # m


@dataclass
class State:
    p: np.ndarray
    v: np.ndarray
    q: np.ndarray
    ba: np.ndarray
    bg: np.ndarray


class ESEKF:
    H = np.hstack([np.eye(3), np.zeros((3, 12))])

    def __init__(self, x0: State, P0: np.ndarray, noise: NoiseSpec | None = None, estimate_biases: bool = True):
        self.x = State(x0.p.copy(), x0.v.copy(), x0.q.copy(), x0.ba.copy(), x0.bg.copy())
        self.P = P0.copy()
        self.n = noise or NoiseSpec()
        self.estimate_biases = estimate_biases
        if not estimate_biases:          # 9-state behaviour: biases fixed and ignored
            self.P[9:, 9:] = 0.0

    def predict(self, f: np.ndarray, w: np.ndarray, dt: float) -> None:
        x = self.x
        R = qmat(x.q)
        f_c = f - x.ba
        w_c = w - x.bg
        a = R @ f_c + GRAVITY
        x.p = x.p + x.v * dt + 0.5 * a * dt * dt
        x.v = x.v + a * dt
        x.q = qnormalize(qmul(x.q, qexp(w_c * dt)))

        F = np.eye(15)
        F[0:3, 3:6] = np.eye(3) * dt
        F[3:6, 6:9] = -R @ skew(f_c) * dt
        F[3:6, 9:12] = -R * dt
        F[6:9, 6:9] = np.eye(3) - skew(w_c) * dt
        F[6:9, 12:15] = -np.eye(3) * dt
        Q = np.zeros((15, 15))
        Q[3:6, 3:6] = np.eye(3) * (self.n.acc * dt) ** 2
        Q[6:9, 6:9] = np.eye(3) * (self.n.gyro * dt) ** 2
        if self.estimate_biases:
            Q[9:12, 9:12] = np.eye(3) * self.n.acc_bias_walk ** 2 * dt
            Q[12:15, 12:15] = np.eye(3) * self.n.gyro_bias_walk ** 2 * dt
        else:
            F[3:6, 9:12] = 0.0
            F[6:9, 12:15] = 0.0
        self.P = F @ self.P @ F.T + Q

    def correct(self, y: np.ndarray, sigma: float) -> np.ndarray:
        """Position fix with isotropic standard deviation sigma. Returns the innovation."""
        H = self.H
        Rm = np.eye(3) * sigma ** 2
        innov = y - self.x.p
        S = H @ self.P @ H.T + Rm
        K = self.P @ H.T @ np.linalg.inv(S)
        dx = K @ innov
        x = self.x
        x.p = x.p + dx[0:3]
        x.v = x.v + dx[3:6]
        x.q = qnormalize(qmul(x.q, qexp(dx[6:9])))
        if self.estimate_biases:
            x.ba = x.ba + dx[9:12]
            x.bg = x.bg + dx[12:15]
        IKH = np.eye(15) - K @ H
        self.P = IKH @ self.P @ IKH.T + K @ Rm @ K.T
        self.P = 0.5 * (self.P + self.P.T)
        return innov
