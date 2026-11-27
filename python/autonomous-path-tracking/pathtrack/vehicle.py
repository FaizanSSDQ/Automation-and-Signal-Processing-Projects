"""Kinematic bicycle model with actuator limits.

The four wheels are lumped into one front and one rear wheel on the centre line.
The reference point is the rear axle (for pure pursuit) and the front axle
position is available for Stanley. With the slip angle neglected, a kinematic
model is accurate at the moderate lateral accelerations of urban driving
(below about 0.4 g).

State: x, y (rear axle, m), yaw (rad), v (m/s), steer (rad).
Inputs: steering-angle command, longitudinal acceleration command.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from dataclasses import dataclass, replace

import numpy as np


@dataclass(frozen=True)
class VehicleParams:
    wheelbase: float = 2.7            # m, a mid-size car
    max_steer: float = np.radians(35)  # rad, road-wheel angle
    max_steer_rate: float = np.radians(60)  # rad/s, steering actuator
    max_accel: float = 3.0            # m/s^2
    max_decel: float = 6.0            # m/s^2


@dataclass(frozen=True)
class VehicleState:
    x: float = 0.0
    y: float = 0.0
    yaw: float = 0.0
    v: float = 0.0
    steer: float = 0.0

    def front_axle(self, wheelbase: float) -> tuple[float, float]:
        return (self.x + wheelbase * np.cos(self.yaw), self.y + wheelbase * np.sin(self.yaw))


class KinematicBicycle:
    """Rear-axle kinematic bicycle, integrated with fixed steps (RK2)."""

    def __init__(self, params: VehicleParams | None = None, state: VehicleState | None = None):
        self.p = params or VehicleParams()
        self.state = state or VehicleState()

    def step(self, steer_cmd: float, accel_cmd: float, dt: float) -> VehicleState:
        p, s = self.p, self.state
        # Actuators: the steering motor is rate limited, both inputs are saturated.
        steer_target = float(np.clip(steer_cmd, -p.max_steer, p.max_steer))
        max_delta = p.max_steer_rate * dt
        steer = s.steer + float(np.clip(steer_target - s.steer, -max_delta, max_delta))
        accel = float(np.clip(accel_cmd, -p.max_decel, p.max_accel))

        def deriv(yaw: float, v: float) -> tuple[float, float, float]:
            return v * np.cos(yaw), v * np.sin(yaw), v * np.tan(steer) / p.wheelbase

        # Midpoint (RK2) integration: accurate enough at 100 Hz, cheap, deterministic.
        _, _, dyaw1 = deriv(s.yaw, s.v)
        v_mid = max(0.0, s.v + 0.5 * accel * dt)
        dx2, dy2, dyaw2 = deriv(s.yaw + 0.5 * dt * dyaw1, v_mid)
        self.state = replace(
            s,
            x=s.x + dt * dx2,
            y=s.y + dt * dy2,
            yaw=float(wrap_angle(s.yaw + dt * dyaw2)),
            v=max(0.0, s.v + accel * dt),
            steer=steer,
        )
        return self.state


def wrap_angle(a):
    """Wrap angles to [-pi, pi)."""
    return (np.asarray(a) + np.pi) % (2 * np.pi) - np.pi
