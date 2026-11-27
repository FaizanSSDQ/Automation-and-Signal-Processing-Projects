"""Tests for the vehicle model, paths, controllers and simulation.

Run from the project folder:  python -m pytest -q

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path as FsPath

import numpy as np
import pytest

sys.path.insert(0, str(FsPath(__file__).resolve().parents[1]))

from pathtrack import (
    KinematicBicycle,
    PurePursuit,
    Stanley,
    VehicleParams,
    VehicleState,
    figure_eight,
    lane_change,
    racetrack,
    simulate,
    tracking_metrics,
)
from pathtrack.paths import from_points

# ---- vehicle model ------------------------------------------------------------------------

def test_straight_line_distance():
    car = KinematicBicycle(state=VehicleState(v=10.0))
    for _ in range(100):
        car.step(0.0, 0.0, 0.01)
    assert car.state.x == pytest.approx(10.0, abs=1e-9)
    assert car.state.y == pytest.approx(0.0, abs=1e-12)


def test_constant_steer_gives_circle_of_radius_L_over_tan_delta():
    p = VehicleParams(max_steer_rate=100.0)
    delta = np.radians(10)
    car = KinematicBicycle(p, VehicleState(v=5.0, steer=delta))
    xs, ys = [], []
    for _ in range(4000):
        car.step(delta, 0.0, 0.005)
        xs.append(car.state.x)
        ys.append(car.state.y)
    xs, ys = np.array(xs), np.array(ys)
    r_expected = p.wheelbase / np.tan(delta)
    cx, cy = 0.0, r_expected                     # starts at origin heading +x, turning left
    r = np.hypot(xs - cx, ys - cy)
    assert np.max(np.abs(r - r_expected)) < 0.01 * r_expected


def test_steering_rate_and_angle_limits():
    p = VehicleParams()
    car = KinematicBicycle(p, VehicleState(v=5.0))
    car.step(np.radians(90), 0.0, 0.1)
    assert car.state.steer == pytest.approx(p.max_steer_rate * 0.1)
    for _ in range(100):
        car.step(np.radians(90), 0.0, 0.1)
    assert car.state.steer == pytest.approx(p.max_steer)


def test_speed_never_negative():
    car = KinematicBicycle(state=VehicleState(v=1.0))
    for _ in range(100):
        car.step(0.0, -10.0, 0.05)
    assert car.state.v == 0.0


# ---- paths ----------------------------------------------------------------------------------

def test_lane_change_geometry():
    p = lane_change(offset=3.5)
    assert p.y[0] == pytest.approx(0.0, abs=1e-9)
    assert p.y[-1] == pytest.approx(3.5, abs=1e-6)
    assert np.all(np.diff(p.s) > 0)


def test_circle_curvature_and_speed_limit():
    t = np.linspace(0, 2 * np.pi, 3000, endpoint=False)
    p = from_points(25 * np.cos(t), 25 * np.sin(t), v_max=30.0, a_lat=3.0, closed=True)
    assert np.median(p.kappa) == pytest.approx(1 / 25, rel=0.01)
    assert np.max(p.v_ref) == pytest.approx(np.sqrt(3.0 * 25), rel=0.02)   # v^2 / R = a_lat


def test_speed_profile_respects_limits():
    p = racetrack()
    lat = p.v_ref ** 2 * np.abs(p.kappa)
    assert np.all(lat <= 3.0 + 1e-6)
    dv2 = np.abs(np.diff(p.v_ref ** 2)) / (2 * np.diff(p.s))
    assert np.all(dv2 <= 2.0 + 1e-6)                                       # |dv/dt| <= 2 m/s^2


# ---- closed loop -------------------------------------------------------------------------------

@pytest.mark.parametrize("ctrl", [PurePursuit, Stanley])
@pytest.mark.parametrize("make_path", [lane_change, figure_eight, racetrack])
def test_controllers_complete_every_path(ctrl, make_path):
    r = simulate(make_path(), ctrl(), t_max=120)
    m = tracking_metrics(r)
    assert r.finished
    assert m.max_cte < 1.6


def test_stanley_tracks_tighter_than_pure_pursuit():
    for make_path in (lane_change, figure_eight, racetrack):
        p = make_path()
        pp = tracking_metrics(simulate(p, PurePursuit(), t_max=120))
        st = tracking_metrics(simulate(p, Stanley(), t_max=120))
        assert st.rms_cte < pp.rms_cte


def test_stanley_recovers_from_an_offset():
    p = lane_change(length=200, offset=0.0)
    start = VehicleState(0.0, 2.0, 0.0, 15.0, 0.0)                 # 2 m left of the lane
    r = simulate(p, Stanley(), start=start, t_max=30)
    assert abs(r.cte[0]) > 1.9
    assert np.max(np.abs(r.cte[r.t > 3.0])) < 0.1


def test_longer_lookahead_cuts_corners_more():
    p = figure_eight()
    errs = [tracking_metrics(simulate(p, PurePursuit(gain=g), t_max=120)).rms_cte for g in (0.3, 0.6, 1.2)]
    assert errs[0] < errs[1] < errs[2]


def test_feedforward_double_counts_in_kinematic_stanley():
    p = figure_eight()
    plain = tracking_metrics(simulate(p, Stanley(), t_max=120)).rms_cte
    ff = tracking_metrics(simulate(p, Stanley(feedforward=True), t_max=120)).rms_cte
    assert ff > 2 * plain


def test_simulation_is_deterministic():
    p = figure_eight()
    a = simulate(p, Stanley(), t_max=120, steer_noise=0.01, seed=3)
    b = simulate(p, Stanley(), t_max=120, steer_noise=0.01, seed=3)
    assert np.array_equal(a.x, b.x)


def test_closed_path_laps():
    p = figure_eight()
    r1 = simulate(p, Stanley(), t_max=200, laps=1)
    r2 = simulate(p, Stanley(), t_max=200, laps=2)
    assert r2.t[-1] == pytest.approx(2 * r1.t[-1], rel=0.05)
