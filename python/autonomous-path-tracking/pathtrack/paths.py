"""Reference paths with arc length, heading, curvature and a speed profile.

Every path is a dense polyline. Heading and curvature come from finite
differences; the speed profile respects a lateral-acceleration limit
(v <= sqrt(a_lat / |kappa|)) and a longitudinal limit, applied with a forward
and a backward pass so the car brakes before a bend instead of in it.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass

import numpy as np


@dataclass
class Path:
    x: np.ndarray
    y: np.ndarray
    s: np.ndarray          # arc length, m
    yaw: np.ndarray        # rad
    kappa: np.ndarray      # curvature, 1/m (positive = left turn)
    v_ref: np.ndarray      # target speed, m/s
    closed: bool = False

    @property
    def length(self) -> float:
        return float(self.s[-1])

    def nearest(self, px: float, py: float, hint: int | None = None, window: int = 400) -> int:
        """Index of the closest path point; searches near `hint` when given (O(window))."""
        n = len(self.x)
        if hint is None:
            idx = np.arange(n)
        else:
            lo, hi = hint - window // 4, hint + window
            idx = np.arange(lo, hi) % n if self.closed else np.arange(max(0, lo), min(n, hi))
        d2 = (self.x[idx] - px) ** 2 + (self.y[idx] - py) ** 2
        return int(idx[int(np.argmin(d2))])

    def cross_track(self, px: float, py: float, i: int) -> float:
        """Signed lateral distance to the path at index i (positive = left of the path)."""
        dx, dy = px - self.x[i], py - self.y[i]
        return float(-np.sin(self.yaw[i]) * dx + np.cos(self.yaw[i]) * dy)


def from_points(x, y, v_max: float, a_lat: float = 3.0, a_long: float = 2.0, closed: bool = False,
                spacing: float = 0.1) -> Path:
    """Resample a polyline to even spacing and compute heading, curvature and speed."""
    x, y = np.asarray(x, float), np.asarray(y, float)
    seg = np.hypot(np.diff(x), np.diff(y))
    s_raw = np.concatenate([[0.0], np.cumsum(seg)])
    s = np.arange(0.0, s_raw[-1], spacing)
    x, y = np.interp(s, s_raw, x), np.interp(s, s_raw, y)
    if closed:
        # periodic differences so heading and curvature are continuous across the seam
        dx = (np.roll(x, -1) - np.roll(x, 1)) / 2
        dy = (np.roll(y, -1) - np.roll(y, 1)) / 2
    else:
        dx, dy = np.gradient(x), np.gradient(y)
    yaw = np.unwrap(np.arctan2(dy, dx))
    dyaw = (np.roll(yaw, -1) - np.roll(yaw, 1)) / 2 if closed else np.gradient(yaw)
    if closed:
        dyaw = (dyaw + np.pi) % (2 * np.pi) - np.pi
    kappa = dyaw / spacing
    kappa = np.convolve(kappa, np.ones(9) / 9, mode="same")   # finite differences are noisy
    v = np.minimum(v_max, np.sqrt(a_lat / np.maximum(np.abs(kappa), 1e-6)))
    for i in range(1, len(v)):                                # accelerate gently
        v[i] = min(v[i], np.sqrt(v[i - 1] ** 2 + 2 * a_long * spacing))
    for i in range(len(v) - 2, -1, -1):                       # brake before bends
        v[i] = min(v[i], np.sqrt(v[i + 1] ** 2 + 2 * a_long * spacing))
    return Path(x, y, s, (yaw + np.pi) % (2 * np.pi) - np.pi, kappa, v, closed)


def lane_change(length: float = 120.0, offset: float = 3.5, v: float = 15.0) -> Path:
    """Straight road with a smooth (cosine) lane change of `offset` metres over 30 m."""
    x = np.linspace(0, length, 2000)
    start, width = 0.4 * length, 30.0
    u = np.clip((x - start) / width, 0, 1)
    y = offset * (1 - np.cos(np.pi * u)) / 2
    return from_points(x, y, v_max=v)


def figure_eight(radius: float = 20.0, v: float = 10.0) -> Path:
    """Lemniscate of Gerono scaled to `radius`: alternating left and right bends."""
    t = np.linspace(0, 2 * np.pi, 4000, endpoint=False)
    return from_points(2 * radius * np.sin(t), radius * np.sin(2 * t), v_max=v, closed=True)


def racetrack(v: float = 18.0) -> Path:
    """A closed circuit with a hairpin, two straights and a chicane (rounded polygon)."""
    pts = np.array([[0, 0], [90, 0], [110, 15], [110, 45], [90, 60], [60, 60], [50, 50],
                    [40, 60], [10, 60], [-10, 45], [-15, 20], [0, 0]], float)
    t = np.linspace(0, 1, len(pts))
    tt = np.linspace(0, 1, 3000)
    # Smooth the corners: interpolate, then a moving average along the closed loop.
    x, y = np.interp(tt, t, pts[:, 0]), np.interp(tt, t, pts[:, 1])
    k = np.ones(151) / 151
    x = np.convolve(np.concatenate([x[-75:], x, x[:75]]), k, mode="valid")[: len(tt)]
    y = np.convolve(np.concatenate([y[-75:], y, y[:75]]), k, mode="valid")[: len(tt)]
    return from_points(x[:-1], y[:-1], v_max=v, closed=True)
