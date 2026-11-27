"""Quaternion and rotation utilities (Hamilton convention, q = [w, x, y, z]).

A quaternion q rotates vectors from the body frame to the navigation frame:
v_nav = R(q) v_body. Orientation errors are small rotation vectors applied on
the right (in the body frame): q_true = q_est (x) exp(dtheta).

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import numpy as np


def skew(v: np.ndarray) -> np.ndarray:
    """[v]x such that skew(a) @ b = a x b."""
    return np.array([[0.0, -v[2], v[1]], [v[2], 0.0, -v[0]], [-v[1], v[0], 0.0]])


def qmul(p: np.ndarray, q: np.ndarray) -> np.ndarray:
    pw, px, py, pz = p
    qw, qx, qy, qz = q
    return np.array([
        pw * qw - px * qx - py * qy - pz * qz,
        pw * qx + px * qw + py * qz - pz * qy,
        pw * qy - px * qz + py * qw + pz * qx,
        pw * qz + px * qy - py * qx + pz * qw,
    ])


def qconj(q: np.ndarray) -> np.ndarray:
    return np.array([q[0], -q[1], -q[2], -q[3]])


def qexp(theta: np.ndarray) -> np.ndarray:
    """Rotation vector (axis * angle) -> unit quaternion."""
    angle = float(np.linalg.norm(theta))
    if angle < 1e-12:
        return np.array([1.0, *(0.5 * theta)]) / np.sqrt(1 + 0.25 * angle ** 2)
    axis = theta / angle
    return np.array([np.cos(angle / 2), *(np.sin(angle / 2) * axis)])


def qlog(q: np.ndarray) -> np.ndarray:
    """Unit quaternion -> rotation vector (shortest rotation)."""
    q = q if q[0] >= 0 else -q
    s = float(np.linalg.norm(q[1:]))
    if s < 1e-12:
        return 2.0 * q[1:]
    return 2.0 * np.arctan2(s, q[0]) * q[1:] / s


def qnormalize(q: np.ndarray) -> np.ndarray:
    q = q / np.linalg.norm(q)
    return q if q[0] >= 0 else -q


def qmat(q: np.ndarray) -> np.ndarray:
    """Rotation matrix of a unit quaternion (body -> navigation)."""
    w, x, y, z = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
        [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
        [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)],
    ])


def euler_to_q(roll: float, pitch: float, yaw: float) -> np.ndarray:
    """ZYX (yaw, pitch, roll) Euler angles -> quaternion."""
    cr, sr = np.cos(roll / 2), np.sin(roll / 2)
    cp, sp = np.cos(pitch / 2), np.sin(pitch / 2)
    cy, sy = np.cos(yaw / 2), np.sin(yaw / 2)
    return np.array([cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy,
                     cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy])


def q_to_euler(q: np.ndarray) -> np.ndarray:
    w, x, y, z = q
    roll = np.arctan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y))
    pitch = np.arcsin(np.clip(2 * (w * y - z * x), -1.0, 1.0))
    yaw = np.arctan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z))
    return np.array([roll, pitch, yaw])
