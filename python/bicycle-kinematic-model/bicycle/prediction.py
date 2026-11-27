"""Short-horizon path prediction: where will this vehicle be in the next few seconds?

Used by driver-assistance and autonomous systems to check for collisions and
plan around other road users. Three predictors, from simple to physical:

    CV    constant velocity: straight line along the current direction of travel
    CTRV  constant turn rate and velocity: an arc with the current yaw rate
    KBM   kinematic bicycle model: integrates the current steering angle (and,
          optionally, its current rate) through the vehicle geometry

Monte-Carlo prediction adds uncertainty in speed and steering and returns
a cloud of predicted endpoints (the prediction "cone").

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import numpy as np

from .kinematic import Geometry, KinematicBicycle


def predict_cv(x, y, heading, v, horizon, dt):
    t = np.arange(0.0, horizon + 1e-9, dt)
    return np.stack([x + v * t * np.cos(heading), y + v * t * np.sin(heading)], axis=1)


def predict_ctrv(x, y, heading, v, yaw_rate, horizon, dt):
    t = np.arange(0.0, horizon + 1e-9, dt)
    if abs(yaw_rate) < 1e-6:
        return predict_cv(x, y, heading, v, horizon, dt)
    th = heading + yaw_rate * t
    return np.stack([x + v / yaw_rate * (np.sin(th) - np.sin(heading)),
                     y - v / yaw_rate * (np.cos(th) - np.cos(heading))], axis=1)


def predict_bicycle(x, y, heading, v, steer, horizon, dt, steer_rate=0.0, geom: Geometry | None = None):
    model = KinematicBicycle(geom, "cg", (x, y, heading, steer))
    n = round(horizon / dt)
    return model.run(np.full(n, v), np.full(n, steer_rate), dt)[:, :2]


SIGMA_STEER = float(np.radians(1.0))   # default steering-angle uncertainty, rad
SIGMA_RATE = float(np.radians(2.0))    # default steering-rate uncertainty, rad/s


def predict_cone(x, y, heading, v, steer, horizon, dt, sigma_v=0.5, sigma_steer=SIGMA_STEER,
                 sigma_rate=SIGMA_RATE, samples=200, seed=0, steer_rate_mean=0.0, geom: Geometry | None = None):
    """Paths of `samples` bicycle predictions with uncertain speed, steering angle and steering rate."""
    rng = np.random.default_rng(seed)
    paths = []
    for _ in range(samples):
        paths.append(predict_bicycle(x, y, heading, max(0.0, v + sigma_v * rng.standard_normal()),
                                     steer + sigma_steer * rng.standard_normal(), horizon, dt,
                                     steer_rate=steer_rate_mean + sigma_rate * rng.standard_normal(), geom=geom))
    return np.array(paths)
