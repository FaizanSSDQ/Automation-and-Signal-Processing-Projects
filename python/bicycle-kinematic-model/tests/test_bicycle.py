"""Tests for the kinematic bicycle model, the manoeuvres, the dynamic reference and the predictors.

Run from the project folder:  python -m pytest -q

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path as FsPath

import numpy as np
import pytest

sys.path.insert(0, str(FsPath(__file__).resolve().parents[1]))

from bicycle import (
    DynamicParams,
    Geometry,
    KinematicBicycle,
    predict_bicycle,
    predict_cone,
    predict_ctrv,
    predict_cv,
    simulate_dynamic,
    steer_for_radius,
)
from bicycle import maneuvers as M

G = Geometry()


def fit_circle(p: np.ndarray) -> tuple[np.ndarray, float]:
    """Least-squares (Kasa) circle fit: returns centre and radius."""
    A = np.c_[2 * p[:, 0], 2 * p[:, 1], np.ones(len(p))]
    c = np.linalg.lstsq(A, (p ** 2).sum(1), rcond=None)[0]
    return c[:2], float(np.sqrt(c[2] + c[0] ** 2 + c[1] ** 2))


# ---------------------------------------------------------------- kinematic model

@pytest.mark.parametrize("ref", ["rear", "cg", "front"])
def test_constant_steer_draws_the_requested_circle(ref):
    d = steer_for_radius(10.0, G, ref)
    s = KinematicBicycle(G, ref, (0, 0, 0, d)).run(np.full(2000, 3.0), 0.0, 0.01)
    _, r = fit_circle(s[:, :2])
    assert r == pytest.approx(10.0, abs=1e-6)


def test_reference_points_need_different_steering_for_the_same_radius():
    rear, cg, front = (steer_for_radius(10.0, G, r) for r in ("rear", "cg", "front"))
    assert rear < cg < front
    assert np.tan(rear) == pytest.approx(G.wheelbase / 10.0)


def test_same_steering_gives_three_concentric_circles():
    d = np.radians(15)
    centres, radii = [], []
    for ref in ("rear", "cg", "front"):
        # start every reference point from the same physical vehicle pose
        x0 = {"rear": 0.0, "cg": G.lr, "front": G.wheelbase}[ref]
        s = KinematicBicycle(G, ref, (x0, 0, 0, d)).run(np.full(3000, 3.0), 0.0, 0.01)
        c, r = fit_circle(s[:, :2])
        centres.append(c)
        radii.append(r)
    for c in centres[1:]:
        assert c == pytest.approx(centres[0], abs=1e-6)       # one instantaneous centre of rotation
    R = G.wheelbase / np.tan(d)
    assert radii == pytest.approx([R, np.hypot(R, G.lr), np.hypot(R, G.wheelbase)], abs=1e-6)


def test_slip_angle_matches_geometry():
    d = np.radians(20)
    assert KinematicBicycle(G, "rear").slip_angle(d) == 0.0
    assert KinematicBicycle(G, "front").slip_angle(d) == pytest.approx(d)
    assert KinematicBicycle(G, "cg").slip_angle(d) == pytest.approx(np.arctan(G.lr / G.wheelbase * np.tan(d)))


def test_straight_line_when_not_steering():
    s = KinematicBicycle(G, "cg").run(np.full(500, 10.0), 0.0, 0.01)
    assert s[-1, :3] == pytest.approx([50.0, 0.0, 0.0], abs=1e-9)


def test_steering_rate_and_angle_are_limited():
    m = KinematicBicycle(G, "cg")
    m.step(5.0, 100.0, 0.1)                       # asks for 10 rad of steering in one step
    assert m.state[3] == pytest.approx(G.max_steer_rate * 0.1)
    m.run(np.full(200, 5.0), 100.0, 0.1)
    assert m.state[3] == pytest.approx(G.max_steer)


def test_invalid_reference_rejected():
    with pytest.raises(ValueError):
        KinematicBicycle(G, "roof")


# ---------------------------------------------------------------- manoeuvres

def test_circle_manoeuvre_after_steering_ramp():
    v, w, dt = M.circle(10.0)
    s = KinematicBicycle(G, "cg").run(v, w, dt)
    _, r = fit_circle(s[100:, :2])
    assert r == pytest.approx(10.0, abs=1e-3)


def test_figure_eight_closes_and_is_symmetric():
    v, w, dt, d0 = M.figure_eight(8.0)
    s = KinematicBicycle(G, "cg", (0, 0, 0, d0)).run(v, w, dt)
    assert np.hypot(*s[-1, :2]) < 0.1                   # back to the start
    assert s[:, 1].max() == pytest.approx(-s[:, 1].min(), abs=0.2)
    assert s[:, 1].max() == pytest.approx(16.0, abs=0.3)   # two 8 m circles stacked


def test_spiral_radius_shrinks():
    v, w, dt = M.spiral()
    s = KinematicBicycle(G, "cg").run(v, w, dt)
    curvature = np.tan(s[:, 3]) / G.wheelbase
    assert np.all(np.diff(curvature) >= -1e-12)
    assert s[-1, 3] < G.max_steer


def test_lane_change_ends_one_lane_over_and_straight():
    v, w, dt = M.lane_change(v=15.0, offset=3.5)
    s = KinematicBicycle(G, "cg").run(v, w, dt)
    assert s[-1, 1] == pytest.approx(3.5, abs=0.05)
    assert s[-1, 2] == pytest.approx(0.0, abs=np.radians(0.2))
    assert abs(s[:, 3]).max() < np.radians(3)          # motorway steering is small


# ---------------------------------------------------------------- dynamic reference

def run_both(vx, radius=30.0, seconds=3.0, dt=0.01):
    d = steer_for_radius(radius, G)
    n = round(seconds / dt)
    dyn = simulate_dynamic(DynamicParams(), vx, np.full(n, d), dt)
    kin = KinematicBicycle(G, "cg", (0, 0, 0, d)).run(np.full(n, vx), 0.0, dt)
    return dyn, kin


def test_dynamic_model_agrees_at_low_lateral_acceleration():
    dyn, kin = run_both(5.0)                            # 0.8 m/s^2
    assert np.hypot(*(dyn[-1, :2] - kin[-1, :2])) < 0.5


def test_kinematic_model_breaks_down_at_high_lateral_acceleration():
    errors = [np.hypot(*(d[-1, :2] - k[-1, :2])) for d, k in (run_both(v) for v in (5, 10, 15, 20))]
    assert np.all(np.diff(errors) > 0)
    assert errors[-1] > 10.0                            # 13 m/s^2: the tyres slip, the model does not


def test_understeer_gradient_predicts_steady_state_yaw_rate():
    p = DynamicParams()
    assert p.understeer_gradient > 0                    # this car understeers
    vx, d = 15.0, np.radians(3)
    dyn = simulate_dynamic(p, vx, np.full(1000, d), 0.01)
    expected = vx * d / (p.wheelbase + p.understeer_gradient * vx ** 2)
    assert dyn[-1, 4] == pytest.approx(expected, rel=1e-3)


# ---------------------------------------------------------------- prediction

def test_cv_and_ctrv_shapes_and_limits():
    a = predict_cv(0, 0, 0, 10, 3.0, 0.1)
    assert a.shape == (31, 2) and a[-1] == pytest.approx([30, 0])
    assert predict_ctrv(0, 0, 0, 10, 0.0, 3.0, 0.1) == pytest.approx(a)
    w = np.pi / 2                                       # one full turn in exactly 4 s
    c = predict_ctrv(0, 0, 0, 10, w, 4.0, 0.01)
    assert c[-1] == pytest.approx([0, 0], abs=1e-6)
    assert np.abs(c[:, 1]).max() == pytest.approx(2 * 10 / w, abs=1e-3)


def test_bicycle_prediction_beats_cv_and_ctrv_when_entering_a_turn():
    rate = np.radians(4)
    truth = KinematicBicycle(G, "cg", (0, 0, 0, np.radians(1))).run(np.full(300, 8.0), rate, 0.01)
    x, y, th, d = truth[0]
    yaw_rate = 8.0 * np.cos(np.arctan(G.lr / G.wheelbase * np.tan(d))) * np.tan(d) / G.wheelbase
    end = truth[-1, :2]
    err = {
        "cv": np.hypot(*(predict_cv(x, y, th + np.arctan(G.lr / G.wheelbase * np.tan(d)), 8.0, 3.0, 0.01)[-1] - end)),
        "ctrv": np.hypot(*(predict_ctrv(x, y, th, 8.0, yaw_rate, 3.0, 0.01)[-1] - end)),
        "bicycle": np.hypot(*(predict_bicycle(x, y, th, 8.0, d, 3.0, 0.01, steer_rate=rate)[-1] - end)),
    }
    assert err["bicycle"] < 1e-6 < err["ctrv"] < err["cv"]


def test_prediction_cone_is_reproducible_and_spreads():
    a = predict_cone(0, 0, 0, 10, 0.05, 3.0, 0.05, samples=50, seed=1)
    b = predict_cone(0, 0, 0, 10, 0.05, 3.0, 0.05, samples=50, seed=1)
    assert a.shape == (50, 61, 2) and np.array_equal(a, b)
    spread = np.linalg.norm(a[:, :, :] - a.mean(0), axis=2).mean(0)
    assert spread[0] == pytest.approx(0.0) and spread[-1] > spread[10] > 0
