"""Steering-rate input sequences that make the model draw classic shapes.

Each manoeuvre returns (speed, steering_rate, dt) arrays to feed
KinematicBicycle.run(). The steering angle needed for a circle of radius R
comes from steer_for_radius(); the steering *rate* is applied only during the
short transitions, as a driver would turn the wheel.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import numpy as np

from .kinematic import Geometry, steer_for_radius


def _ramp(w: np.ndarray, k0: int, delta: float, rate: float, dt: float) -> int:
    """Write a constant steering rate that changes the angle by `delta`, starting at k0. Returns end index."""
    n = max(1, round(abs(delta) / (rate * dt)))
    w[k0:k0 + n] = delta / (n * dt)
    return k0 + n


def circle(radius: float = 10.0, v: float | None = None, period: float = 20.0, dt: float = 0.01,
           geom: Geometry | None = None):
    """One lap of a circle of `radius`, entered with a short steering ramp."""
    geom = geom or Geometry()
    v = v if v is not None else 2 * np.pi * radius / period
    d = steer_for_radius(radius, geom)
    n = round((period + 1.0) / dt)
    w = np.zeros(n)
    _ramp(w, 0, d, geom.max_steer_rate * 0.9, dt)
    return np.full(n, v), w, dt


def figure_eight(radius: float = 8.0, period: float = 30.0, dt: float = 0.01, geom: Geometry | None = None):
    """Two circles in opposite directions: left lap, steer through to the right, right lap.

    The course classic: the steering is pre-turned to the left-circle angle
    at the start, and reversed once a full left circle is done.
    """
    geom = geom or Geometry()
    v = 2 * 2 * np.pi * radius / period
    d = steer_for_radius(radius, geom)
    n = round(period / dt)
    w = np.zeros(n)
    k = round(n / 2)
    swing = round(2 * d / (geom.max_steer_rate * 0.95 * dt))
    k0 = k - swing // 2                       # centre the swing on the crossing point
    _ramp(w, k0, -2 * d, geom.max_steer_rate * 0.95, dt)
    return np.full(n, v), w, dt, d


def spiral(v: float = 6.0, duration: float = 30.0, dt: float = 0.01, geom: Geometry | None = None):
    """Steering slowly wound in: the radius shrinks and the path spirals inwards."""
    geom = geom or Geometry()
    n = round(duration / dt)
    rate = geom.max_steer / duration * 0.9
    return np.full(n, v), np.full(n, rate), dt


def lane_change(v: float = 15.0, offset: float = 3.5, dt: float = 0.01, geom: Geometry | None = None):
    """A sine-shaped steering input: left, right, straight, the motorway lane change."""
    geom = geom or Geometry()
    T = 3.0                                    # duration of the manoeuvre, s
    n = round((T + 4.0) / dt)
    t = np.arange(n) * dt
    # lateral offset y(t) = offset * (t/T - sin(2 pi t/T)/(2 pi)): yaw ~ y'/v, curvature ~ y''/v^2
    tt = np.clip((t - 1.0) / T, 0, 1)
    curv = offset * (2 * np.pi / T ** 2) * np.sin(2 * np.pi * tt) / v ** 2
    curv[(t < 1.0) | (t > 1.0 + T)] = 0.0
    steer = np.arctan(geom.wheelbase * curv)
    w = np.gradient(steer, dt)
    return np.full(n, v), w, dt
