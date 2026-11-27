"""Kinematic bicycle model with a selectable reference point.

The two front wheels are lumped into one steerable wheel and the two rear
wheels into one fixed wheel. With no tyre slip, each wheel moves along its own
heading and the vehicle turns about the instantaneous centre where the wheel
axes meet. The equations depend on which point is tracked:

    rear axle :  x' = v cos(th),        y' = v sin(th),        th' = v tan(d) / L
    centre (CG):  x' = v cos(th + b),   y' = v sin(th + b),    th' = v cos(b) tan(d) / L
                 with the slip angle  b = atan(lr tan(d) / L)
    front axle:  x' = v cos(th + d),    y' = v sin(th + d),    th' = v sin(d) / L

Inputs are the speed v and the steering rate w (the steering angle d is a
state, so the angle cannot jump, as with a real steering column).
Integrated with the classical fourth-order Runge-Kutta method.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class Geometry:
    wheelbase: float = 2.7        # L, m
    lr: float = 1.5               # rear axle to centre of gravity, m
    max_steer: float = np.radians(35)
    max_steer_rate: float = np.radians(70)   # rad/s

    @property
    def lf(self) -> float:
        return self.wheelbase - self.lr


class KinematicBicycle:
    """State [x, y, heading, steer]; `ref` is 'rear', 'cg' or 'front'."""

    def __init__(self, geom: Geometry | None = None, ref: str = "cg", state=(0.0, 0.0, 0.0, 0.0)):
        if ref not in ("rear", "cg", "front"):
            raise ValueError("ref must be 'rear', 'cg' or 'front'")
        self.g = geom or Geometry()
        self.ref = ref
        self.state = np.array(state, dtype=float)

    def slip_angle(self, steer: float) -> float:
        """Angle between the velocity of the reference point and the vehicle heading."""
        if self.ref == "rear":
            return 0.0
        if self.ref == "front":
            return float(steer)
        return float(np.arctan(self.g.lr * np.tan(steer) / self.g.wheelbase))

    def derivative(self, s: np.ndarray, v: float, steer_rate: float) -> np.ndarray:
        _, _, th, d = s
        L = self.g.wheelbase
        if self.ref == "rear":
            yaw_rate = v * np.tan(d) / L
        elif self.ref == "front":
            yaw_rate = v * np.sin(d) / L
        else:
            yaw_rate = v * np.cos(self.slip_angle(d)) * np.tan(d) / L
        b = self.slip_angle(d)
        return np.array([v * np.cos(th + b), v * np.sin(th + b), yaw_rate, steer_rate])

    def step(self, v: float, steer_rate: float, dt: float) -> np.ndarray:
        g = self.g
        w = float(np.clip(steer_rate, -g.max_steer_rate, g.max_steer_rate))
        s = self.state
        k1 = self.derivative(s, v, w)
        k2 = self.derivative(s + 0.5 * dt * k1, v, w)
        k3 = self.derivative(s + 0.5 * dt * k2, v, w)
        k4 = self.derivative(s + dt * k3, v, w)
        s = s + dt / 6.0 * (k1 + 2 * k2 + 2 * k3 + k4)
        s[3] = float(np.clip(s[3], -g.max_steer, g.max_steer))
        self.state = s
        return s

    def run(self, v, steer_rate, dt: float) -> np.ndarray:
        """Apply input sequences (arrays or scalars broadcast to the same length); return all states."""
        v = np.atleast_1d(v).astype(float)
        w = np.atleast_1d(steer_rate).astype(float)
        n = max(len(v), len(w))
        v = np.broadcast_to(v, (n,)) if len(v) == 1 else v
        w = np.broadcast_to(w, (n,)) if len(w) == 1 else w
        out = np.zeros((n + 1, 4))
        out[0] = self.state
        for k in range(n):
            out[k + 1] = self.step(v[k], w[k], dt)
        return out


def steer_for_radius(radius: float, geom: Geometry, ref: str = "cg") -> float:
    """Steady steering angle that makes the reference point move on a circle of `radius`."""
    L, lr = geom.wheelbase, geom.lr
    if ref == "rear":
        return float(np.arctan(L / radius))
    if ref == "front":
        return float(np.arcsin(L / radius))
    # CG: R^2 = lr^2 + (L / tan d)^2
    return float(np.arctan(L / np.sqrt(radius ** 2 - lr ** 2)))
