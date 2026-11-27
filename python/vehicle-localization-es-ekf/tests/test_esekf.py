"""Tests for the rotations, the simulated sensors and the ES-EKF.

Run from the project folder:  python -m pytest -q

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from esekf import SensorSpec, make_truth, rmse, run_filter, simulate_sensors
from esekf.rotations import euler_to_q, q_to_euler, qconj, qexp, qlog, qmat, qmul

# ---- rotations ------------------------------------------------------------------------------

def test_exp_log_round_trip():
    rng = np.random.default_rng(1)
    for _ in range(50):
        th = rng.normal(size=3) * 0.8
        assert np.allclose(qlog(qexp(th)), th, atol=1e-12)


def test_rotation_matrix_is_orthonormal_and_matches_quaternion_product():
    q = euler_to_q(0.3, -0.2, 1.1)
    R = qmat(q)
    assert np.allclose(R @ R.T, np.eye(3), atol=1e-12)
    assert np.linalg.det(R) == pytest.approx(1.0)
    v = np.array([1.0, 2.0, 3.0])
    rotated = qmul(qmul(q, np.array([0.0, *v])), qconj(q))[1:]
    assert np.allclose(R @ v, rotated, atol=1e-12)


def test_euler_round_trip():
    assert np.allclose(q_to_euler(euler_to_q(0.1, -0.4, 2.5)), [0.1, -0.4, 2.5], atol=1e-12)


# ---- shared scenarios (computed once) ----------------------------------------------------------

@pytest.fixture(scope="module")
def truth():
    return make_truth(duration=120.0)


@pytest.fixture(scope="module")
def nominal(truth):
    meas = simulate_sensors(truth)
    return meas, run_filter(truth, meas)


# ---- the simulation is self-consistent ---------------------------------------------------------

def test_truth_specific_force_at_rest_is_gravity_upwards(truth):
    # |f| = |a - g|: never zero on a moving vehicle, close to 9.81 m/s^2 on average
    assert np.mean(np.linalg.norm(truth.f_body, axis=1)) == pytest.approx(9.81, rel=0.02)


def test_perfect_imu_dead_reckoning_follows_the_truth(truth):
    spec = SensorSpec(acc_bias=(0, 0, 0), gyro_bias=(0, 0, 0), acc_noise=0.0, gyro_noise=0.0)
    r = run_filter(truth, simulate_sensors(truth, spec), use_gnss=False, use_lidar=False)
    assert np.linalg.norm(r.err[20 * 200, 0:3]) < 0.1        # under 10 cm after 20 s


def test_lidar_measurements_live_in_the_lidar_frame(truth):
    meas = simulate_sensors(truth)
    k = np.searchsorted(truth.t, meas.lidar_t[50])
    assert np.linalg.norm(meas.lidar_p[50] - truth.p[k]) > 0.5      # not already in the IMU frame


# ---- the filter -----------------------------------------------------------------------------------

def test_fusion_accuracy(nominal):
    _, r = nominal
    assert rmse(r.err[:, 0:3]) < 0.2
    assert np.degrees(rmse(r.err[:, 6:9])) < 2.0


def test_fusion_beats_each_sensor_alone(truth, nominal):
    meas, fused = nominal
    gnss_only = run_filter(truth, meas, use_lidar=False)
    imu_only = run_filter(truth, meas, use_gnss=False, use_lidar=False)
    assert rmse(fused.err[:, 0:3]) < rmse(gnss_only.err[:, 0:3])
    assert rmse(imu_only.err[:, 0:3]) > 100 * rmse(fused.err[:, 0:3])


def test_biases_are_estimated(nominal):
    meas, r = nominal
    assert np.allclose(r.bg[-1], meas.spec.gyro_bias, atol=3e-4)       # 0.02 deg/s
    assert np.allclose(r.ba[-1], meas.spec.acc_bias, atol=5e-3)        # 5 mm/s^2


def test_filter_is_consistent(nominal):
    _, r = nominal
    assert 4.0 < np.mean(r.nees[200:]) < 13.0                          # 9 degrees of freedom
    inside = np.mean(np.abs(r.err[:, 0:3]) <= 3 * r.sigma[:, 0:3])
    assert inside > 0.99


def test_ignoring_biases_breaks_consistency(truth, nominal):
    meas, good = nominal
    bad = run_filter(truth, meas, estimate_biases=False)
    assert rmse(bad.err[:, 0:3]) > 5 * rmse(good.err[:, 0:3])
    assert np.mean(bad.nees) > 100


def test_wrong_lidar_calibration_is_exposed_by_nees(truth, nominal):
    meas, good = nominal
    bad = run_filter(truth, meas, lidar_rpy_assumed=(0.05, 0.05, 0.05))   # yaw off by 0.05 rad
    assert rmse(bad.err[:, 0:3]) > 10 * rmse(good.err[:, 0:3])
    assert np.mean(bad.nees) > 100 * np.mean(good.nees)


def test_outage_drift_stays_inside_the_uncertainty_and_recovers(truth):
    spec = SensorSpec(gnss_outage=(60, 90), lidar_outage=(60, 90))
    r = run_filter(truth, simulate_sensors(truth, spec))
    e = np.linalg.norm(r.err[:, 0:3], axis=1)
    s3 = 3 * np.linalg.norm(r.sigma[:, 0:3], axis=1)
    k0, k1 = 60 * 200, 90 * 200
    assert e[k1 - 1] > 5 * e[k0]                                       # it drifts while blind ...
    assert s3[k1 - 1] > 10 * s3[k0]                                    # ... and knows it
    assert np.all(e[k0:k1] < s3[k0:k1])
    assert e[k1 + 2 * 200] < 0.3                                       # back within 2 s
