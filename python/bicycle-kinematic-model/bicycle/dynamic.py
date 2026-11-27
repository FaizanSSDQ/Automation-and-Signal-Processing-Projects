"""Linear dynamic single-track model: the reference the kinematic model is checked against.

States: x, y, heading, lateral velocity vy and yaw rate r (body frame, at the
centre of gravity), at constant forward speed vx. The tyres generate lateral
force in proportion to their slip angle (linear tyre, valid below about 0.4 g):

    a_f = d - (vy + lf r) / vx,      a_r = -(vy - lr r) / vx
    F_f = C_f a_f,                   F_r = C_r a_r
    m (vy' + vx r) = F_f + F_r,      I_z r' = lf F_f - lr F_r

At low lateral acceleration the tyre slip angles are tiny and this model
reduces to the kinematic one; at higher lateral acceleration the car
understeers (it turns less than the kinematic model predicts).

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class DynamicParams:
    mass: float = 1500.0          # kg
    iz: float = 2250.0            # kg m^2
    lf: float = 1.2               # m
    lr: float = 1.5               # m
    cf: float = 80000.0           # N/rad, front axle cornering stiffness
    cr: float = 80000.0           # N/rad, rear axle

    @property
    def wheelbase(self) -> float:
        return self.lf + self.lr

    @property
    def understeer_gradient(self) -> float:
        """K in rad / (m/s^2): steady state  d = L / R + K a_y."""
        return self.mass / self.wheelbase * (self.lr / self.cf - self.lf / self.cr)


def simulate_dynamic(p: DynamicParams, vx: float, steer: np.ndarray, dt: float,
                     state=(0.0, 0.0, 0.0, 0.0, 0.0)) -> np.ndarray:
    """Integrate (RK4) with a steering sequence; returns rows [x, y, heading, vy, r]."""
    def f(s, d):
        _, _, th, vy, r = s
        af = d - (vy + p.lf * r) / vx
        ar = -(vy - p.lr * r) / vx
        ff, fr = p.cf * af, p.cr * ar
        vy_dot = (ff + fr) / p.mass - vx * r
        r_dot = (p.lf * ff - p.lr * fr) / p.iz
        c, s_ = np.cos(th), np.sin(th)
        return np.array([vx * c - vy * s_, vx * s_ + vy * c, r, vy_dot, r_dot])

    out = np.zeros((len(steer) + 1, 5))
    out[0] = state
    s = np.array(state, float)
    for k, d in enumerate(steer):
        k1 = f(s, d)
        k2 = f(s + 0.5 * dt * k1, d)
        k3 = f(s + 0.5 * dt * k2, d)
        k4 = f(s + dt * k3, d)
        s = s + dt / 6.0 * (k1 + 2 * k2 + 2 * k3 + k4)
        out[k + 1] = s
    return out
