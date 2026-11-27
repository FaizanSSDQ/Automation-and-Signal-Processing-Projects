"""Lateral controllers (pure pursuit, Stanley) and a longitudinal PI speed controller.

Pure pursuit (geometric, rear axle): pick a look-ahead point on the path at
distance Ld = k * v + Ld_min and steer along the circular arc through it:
    delta = atan(2 L sin(alpha) / Ld)
where alpha is the angle to that point in the vehicle frame. Smooth, but it
cuts corners, more so with a longer look-ahead.

Stanley (front axle, from Stanford's DARPA Grand Challenge car): correct the
heading error and the cross-track error e at the front axle:
    delta = psi_e + atan(k e / (k_soft + v))
It converges exponentially to the path at any speed, but reacts sharply to
noise at low speed (k_soft damps that). Measured at the front axle, the heading
error already contains the steering a bend needs; adding a curvature
feed-forward on top counts it twice (kept as an option to show the effect).

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass, field

import numpy as np

from .paths import Path
from .vehicle import VehicleState, wrap_angle


@dataclass
class PurePursuit:
    gain: float = 0.6        # look-ahead seconds: Ld = gain * v + min_lookahead
    min_lookahead: float = 3.0
    wheelbase: float = 2.7
    _hint: int | None = field(default=None, repr=False)

    def __call__(self, path: Path, s: VehicleState) -> tuple[float, int]:
        i = path.nearest(s.x, s.y, self._hint)
        self._hint = i
        ld = self.gain * s.v + self.min_lookahead
        j = i
        n = len(path.x)
        while np.hypot(path.x[j % n] - s.x, path.y[j % n] - s.y) < ld:
            j += 1
            if not path.closed and j >= n - 1:
                j = n - 1
                break
            if j - i > n:
                break
        j %= n
        alpha = wrap_angle(np.arctan2(path.y[j] - s.y, path.x[j] - s.x) - s.yaw)
        steer = float(np.arctan2(2 * self.wheelbase * np.sin(alpha), ld))
        return steer, i


@dataclass
class Stanley:
    gain: float = 1.2
    k_soft: float = 1.0       # m/s, keeps the gain finite at low speed
    wheelbase: float = 2.7
    feedforward: bool = False  # curvature feed-forward: double-counts in this kinematic form
    _hint: int | None = field(default=None, repr=False)

    def __call__(self, path: Path, s: VehicleState) -> tuple[float, int]:
        fx, fy = s.front_axle(self.wheelbase)
        i = path.nearest(fx, fy, self._hint)
        self._hint = i
        e = path.cross_track(fx, fy, i)
        # Interpolate the path heading between samples: without this, psi_e steps
        # every time the nearest point advances and the steering chatters.
        j = (i + 1) % len(path.x) if path.closed else min(i + 1, len(path.x) - 1)
        spacing = path.s[1] - path.s[0]
        along = np.cos(path.yaw[i]) * (fx - path.x[i]) + np.sin(path.yaw[i]) * (fy - path.y[i])
        frac = float(np.clip(along / spacing, -1.0, 1.0))
        yaw_path = path.yaw[i] + frac * float(wrap_angle(path.yaw[j] - path.yaw[i]))
        psi_e = float(wrap_angle(yaw_path - s.yaw))
        steer = psi_e + np.arctan2(-self.gain * e, self.k_soft + s.v)
        if self.feedforward:
            steer += np.arctan(self.wheelbase * path.kappa[i])
        # The rear-axle index is what the metrics use.
        return float(steer), path.nearest(s.x, s.y, i)


@dataclass
class SpeedPI:
    kp: float = 1.2
    ki: float = 0.3
    _integral: float = 0.0

    def __call__(self, v_ref: float, v: float, dt: float) -> float:
        err = v_ref - v
        self._integral = float(np.clip(self._integral + err * dt, -5.0, 5.0))   # anti-windup
        return self.kp * err + self.ki * self._integral
