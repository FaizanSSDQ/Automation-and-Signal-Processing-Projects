"""Ground-truth vehicle trajectory with exact derivatives.

Position is an analytic curve (a large figure eight with gentle hills), so
velocity and acceleration are exact. The vehicle faces along its velocity
(yaw and pitch from the direction of travel) and rolls slightly in the bends.
Body angular rates come from the rotation between two instants half a sample
either side of each sample time, which is exact to second order.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass

import numpy as np

from .rotations import euler_to_q, qconj, qlog, qmat, qmul

GRAVITY = np.array([0.0, 0.0, -9.81])


@dataclass
class Truth:
    t: np.ndarray        # s
    p: np.ndarray        # n x 3, m (east, north, up)
    v: np.ndarray        # n x 3, m/s
    a: np.ndarray        # n x 3, m/s^2
    q: np.ndarray        # n x 4, body -> navigation
    w_body: np.ndarray   # n x 3, rad/s
    f_body: np.ndarray   # n x 3, specific force in the body frame, m/s^2


A, B, H = 150.0, 90.0, 3.0      # m: figure-eight half-width, half-height, hill height
T_LAP = 100.0                   # s per figure eight


def _pva(t):
    w = 2 * np.pi / T_LAP
    p = np.stack([A * np.sin(w * t), B * np.sin(2 * w * t), H * np.sin(3 * w * t)], axis=-1)
    v = np.stack([A * w * np.cos(w * t), 2 * B * w * np.cos(2 * w * t), 3 * H * w * np.cos(3 * w * t)], axis=-1)
    a = np.stack([-A * w ** 2 * np.sin(w * t), -4 * B * w ** 2 * np.sin(2 * w * t),
                  -9 * H * w ** 2 * np.sin(3 * w * t)], axis=-1)
    return p, v, a


def _attitude(t):
    _, v, a = _pva(np.atleast_1d(t))
    yaw = np.arctan2(v[:, 1], v[:, 0])
    pitch = np.arctan2(v[:, 2], np.hypot(v[:, 0], v[:, 1]))
    # roll into the bend, proportional to the lateral acceleration (like a body leaning on its springs)
    lat = (v[:, 0] * a[:, 1] - v[:, 1] * a[:, 0]) / np.maximum(np.hypot(v[:, 0], v[:, 1]), 0.1)
    roll = 0.01 * lat
    return np.array([euler_to_q(r, -pp, y) for r, pp, y in zip(roll, pitch, yaw)])


def make_truth(duration: float = 200.0, rate: float = 200.0) -> Truth:
    t = np.arange(0.0, duration, 1.0 / rate)
    p, v, a = _pva(t)
    q = _attitude(t)
    h = 0.5 / rate
    q_lo, q_hi = _attitude(t - h), _attitude(t + h)
    w_body = np.array([qlog(qmul(qconj(ql), qh)) for ql, qh in zip(q_lo, q_hi)]) / (2 * h)
    f_body = np.array([qmat(qk).T @ (ak - GRAVITY) for qk, ak in zip(q, a)])
    return Truth(t, p, v, a, q, w_body, f_body)
