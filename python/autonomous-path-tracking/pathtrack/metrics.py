"""Tracking quality figures from a simulation run.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass

import numpy as np

from .simulate import SimResult


@dataclass(frozen=True)
class TrackingMetrics:
    rms_cte: float          # m
    max_cte: float          # m
    rms_heading: float      # deg
    max_steer_rate: float   # deg/s, how hard the controller works the actuator
    lap_time: float         # s
    max_lat_acc: float      # m/s^2, comfort / grip


def tracking_metrics(r: SimResult, wheelbase: float = 2.7, settle: float = 0.0) -> TrackingMetrics:
    """Metrics after `settle` seconds (to exclude an initial offset)."""
    m = r.t >= settle
    dt = r.t[1] - r.t[0]
    lat = r.v ** 2 * np.tan(r.steer) / wheelbase
    return TrackingMetrics(
        rms_cte=float(np.sqrt(np.mean(r.cte[m] ** 2))),
        max_cte=float(np.max(np.abs(r.cte[m]))),
        rms_heading=float(np.degrees(np.sqrt(np.mean(r.heading_err[m] ** 2)))),
        max_steer_rate=float(np.degrees(np.max(np.abs(np.diff(r.steer[m]))) / dt)),
        lap_time=float(r.t[-1]),
        max_lat_acc=float(np.max(np.abs(lat[m]))),
    )
