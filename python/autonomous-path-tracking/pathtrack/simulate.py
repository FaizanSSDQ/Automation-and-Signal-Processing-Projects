"""Closed-loop simulation: path + lateral controller + speed controller + vehicle.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass

import numpy as np

from .controllers import SpeedPI
from .paths import Path
from .vehicle import KinematicBicycle, VehicleParams, VehicleState, wrap_angle


@dataclass
class SimResult:
    t: np.ndarray
    x: np.ndarray
    y: np.ndarray
    yaw: np.ndarray
    v: np.ndarray
    steer: np.ndarray
    cte: np.ndarray          # cross-track error at the rear axle, m (positive = left of the path)
    heading_err: np.ndarray  # path heading minus vehicle heading, rad
    idx: np.ndarray          # nearest path index at each step
    finished: bool           # reached the end of the path (or completed the laps)


def simulate(path: Path, controller, *, dt: float = 0.01, t_max: float = 60.0,
             start: VehicleState | None = None, params: VehicleParams | None = None,
             laps: float = 1.0, steer_noise: float = 0.0, seed: int = 0) -> SimResult:
    """Drive the path (`laps` times if it is closed) and log every step.

    steer_noise adds Gaussian noise (rad) to every steering command, standing in
    for localisation noise and actuator imperfections.
    """
    rng = np.random.default_rng(seed)
    params = params or VehicleParams()
    if start is None:
        start = VehicleState(path.x[0], path.y[0], float(path.yaw[0]), float(path.v_ref[0]), 0.0)
    car = KinematicBicycle(params, start)
    speed = SpeedPI()
    spacing = path.s[1] - path.s[0]
    n = len(path.x)
    rows, idx = [], []
    travelled, last_i = 0.0, None
    finished = False
    for k in range(int(t_max / dt)):
        s = car.state
        steer, i = controller(path, s)
        if last_i is not None:
            di = i - last_i
            if path.closed:
                di = (di + n // 2) % n - n // 2           # shortest way round the loop
            travelled += max(di, 0) * spacing
        last_i = i
        rows.append((k * dt, s.x, s.y, s.yaw, s.v, s.steer, path.cross_track(s.x, s.y, i),
                     float(wrap_angle(path.yaw[i] - s.yaw))))
        idx.append(i)
        if travelled >= laps * path.length - 0.5 or (not path.closed and i >= n - 2):
            finished = True
            break
        accel = speed(float(path.v_ref[i]), s.v, dt)
        car.step(steer + steer_noise * rng.standard_normal(), accel, dt)
    log = np.array(rows)
    return SimResult(t=log[:, 0], x=log[:, 1], y=log[:, 2], yaw=log[:, 3], v=log[:, 4],
                     steer=log[:, 5], cte=log[:, 6], heading_err=log[:, 7], idx=np.array(idx),
                     finished=finished)
