"""Regenerate the README figures and the animation:  python tools/make_figures.py

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import io
import logging
import sys
from pathlib import Path as FsPath

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon
from PIL import Image

ROOT = FsPath(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
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

logging.getLogger("matplotlib.font_manager").setLevel(logging.ERROR)
DOCS = ROOT / "docs"
BLUE, ORANGE, GREEN, PURPLE, GREY, DARK = "#2b6cb0", "#dd6b20", "#2f855a", "#6b46c1", "#a0aec0", "#2d3748"
plt.rcParams.update({"font.size": 11, "axes.spines.top": False, "axes.spines.right": False})
G = Geometry()
P = DynamicParams()


def car(ax, x_rear, y_rear, th, d, color=DARK, alpha=1.0, z=5):
    """Top view of the vehicle, placed by its rear-axle centre."""
    c, s = np.cos(th), np.sin(th)
    R = np.array([[c, -s], [s, c]])
    body = np.array([[-0.9, -0.85], [G.wheelbase + 0.9, -0.85], [G.wheelbase + 0.9, 0.85], [-0.9, 0.85]])
    ax.add_patch(Polygon(body @ R.T + [x_rear, y_rear], closed=True, fc="white", ec=color, lw=1.4,
                         alpha=alpha, zorder=z))
    wheel = np.array([[-0.35, -0.12], [0.35, -0.12], [0.35, 0.12], [-0.35, 0.12]])
    for wx, wy, ang in ((0, -0.75, 0), (0, 0.75, 0), (G.wheelbase, -0.75, d), (G.wheelbase, 0.75, d)):
        cw, sw = np.cos(ang), np.sin(ang)
        pts = wheel @ np.array([[cw, -sw], [sw, cw]]).T + [wx, wy]
        ax.add_patch(Polygon(pts @ R.T + [x_rear, y_rear], closed=True, fc=color, ec=color, alpha=alpha, zorder=z + 1))


def fig_reference_points():
    d = np.radians(30)
    fig, (a, b) = plt.subplots(1, 2, figsize=(14, 5.8), gridspec_kw={"width_ratios": [1.3, 1]})
    styles = {"rear": (BLUE, "-", 0.0, "rear axle"), "cg": (GREEN, "--", G.lr, "centre of gravity"),
              "front": (ORANGE, ":", G.wheelbase, "front axle")}
    for ref, (col, ls, x0, name) in styles.items():
        s = KinematicBicycle(G, ref, (x0, 0, 0, d)).run(np.full(4000, 3.0), 0.0, 0.01)
        r = np.hypot(G.wheelbase / np.tan(d), x0)
        a.plot(s[:, 0], s[:, 1], color=col, ls=ls, lw=2, label=f"{name}: R = {r:.2f} m")
    R = G.wheelbase / np.tan(d)
    a.plot([0], [R], "+", color=DARK, ms=12, mew=2)
    a.annotate("instantaneous centre of rotation", (0, R), (-4.0, R + 0.5), fontsize=10, color=DARK)
    for xx in (0.0, G.lr, G.wheelbase):
        a.plot([xx, 0], [0, R], color=GREY, lw=0.8, zorder=1)
    car(a, 0, 0, 0, d)
    a.set_aspect("equal")
    a.set_title("Same steering (30°), three reference points, three circles")
    a.set_xlabel("x (m)")
    a.set_ylabel("y (m)")
    a.legend(loc="upper left", bbox_to_anchor=(1.0, 1.0), fontsize=9, frameon=False)

    deg = np.linspace(0.5, 35, 200)
    beta = np.degrees(np.arctan(G.lr / G.wheelbase * np.tan(np.radians(deg))))
    b.plot(deg, deg, color=ORANGE, ls=":", lw=2)
    b.plot(deg, beta, color=GREEN, ls="--", lw=2)
    b.plot(deg, 0 * deg, color=BLUE, lw=2)
    b.text(30, 31.5, "front axle  β = δ", color=DARK, fontsize=10, ha="right")
    b.text(34, beta[-1] + 1.2, "centre of gravity\nβ = atan(lr·tanδ / L)", color=DARK, fontsize=10, ha="right")
    b.text(34, 1.0, "rear axle  β = 0", color=DARK, fontsize=10, ha="right")
    b.set_xlabel("steering angle δ (deg)")
    b.set_ylabel("slip angle β of the tracked point (deg)")
    b.set_title("Slip angle: direction of travel vs. heading")
    b.grid(alpha=0.25)
    fig.tight_layout()
    fig.savefig(DOCS / "reference_points.png", dpi=130)
    plt.close(fig)


def fig_maneuvers():
    fig, axes = plt.subplots(1, 4, figsize=(17, 4.6))
    v, w, dt = M.circle(10.0)
    s1 = KinematicBicycle(G, "cg").run(v, w, dt)
    v, w, dt, d0 = M.figure_eight(8.0)
    s2 = KinematicBicycle(G, "cg", (0, 0, 0, d0)).run(v, w, dt)
    v, w, dt = M.spiral()
    s3 = KinematicBicycle(G, "cg").run(v, w, dt)
    v, w, dt = M.lane_change()
    s4 = KinematicBicycle(G, "cg").run(v, w, dt)
    items = [(s1, "Circle, R = 10 m", BLUE), (s2, "Figure eight, R = 8 m", GREEN),
             (s3, "Spiral: steering wound in", PURPLE), (s4, "Lane change at 15 m/s", ORANGE)]
    for ax, (s, title, col) in zip(axes, items):
        ax.plot(s[:, 0], s[:, 1], color=col, lw=2)
        ax.plot(s[0, 0], s[0, 1], "o", color=DARK, ms=6)
        ax.set_title(title)
        ax.set_xlabel("x (m)")
        ax.grid(alpha=0.25)
        if s is not s4:
            ax.set_aspect("equal")
    for yy in (-1.75, 1.75, 5.25):
        axes[3].axhline(yy, color=GREY, ls="--", lw=1)
    axes[3].set_ylim(-3, 7)
    axes[0].set_ylabel("y (m)")
    inset = axes[3].inset_axes([0.55, 0.12, 0.42, 0.3])
    inset.plot(np.arange(len(s4)) * 0.01, np.degrees(s4[:, 3]), color=DARK, lw=1.2)
    inset.set_title("steering (deg)", fontsize=8)
    inset.tick_params(labelsize=7)
    fig.tight_layout()
    fig.savefig(DOCS / "maneuvers.png", dpi=130)
    plt.close(fig)


def kin_vs_dyn(vx, radius=30.0, seconds=3.0, dt=0.01):
    d = steer_for_radius(radius, G)
    n = round(seconds / dt)
    return simulate_dynamic(P, vx, np.full(n, d), dt), KinematicBicycle(G, "cg", (0, 0, 0, d)).run(np.full(n, vx), 0.0, dt)


def fig_validity():
    fig, (a, b) = plt.subplots(1, 2, figsize=(14, 5.4))
    for vx, col in ((5, BLUE), (15, GREEN), (25, ORANGE)):
        dyn, kin = kin_vs_dyn(vx, seconds=30.0 * np.pi / 2 / vx)       # a quarter circle each
        a.plot(kin[:, 0], kin[:, 1], color=GREY, lw=3, zorder=1)
        a.plot(dyn[:, 0], dyn[:, 1], color=col, lw=2, ls="--")
        a.text(dyn[-1, 0] + 0.6, dyn[-1, 1], f"{vx} m/s ({vx ** 2 / 30:.1f} m/s²)", color=DARK, fontsize=10,
               va="center")
    a.text(44, 1.0, "grey: kinematic model,\nthe same arc at every speed", color=DARK, fontsize=10, ha="right")
    a.set_aspect("equal")
    a.set_xlim(-1, 45)
    a.set_title("Steering for R = 30 m: the tyre model widens the turn")
    a.set_xlabel("x (m)")
    a.set_ylabel("y (m)")
    a.grid(alpha=0.25)

    speeds = np.linspace(2, 26, 25)
    ay = speeds ** 2 / 30.0
    err = []
    for vx in speeds:
        dyn, kin = kin_vs_dyn(vx)
        err.append(np.hypot(*(dyn[-1, :2] - kin[-1, :2])))
    b.semilogy(ay, err, color=BLUE, lw=2, marker="o", ms=5)
    limit = float(np.interp(0.5, err, ay))              # where the error reaches 0.5 m in 3 s
    b.axhline(0.5, color=GREY, ls="--", lw=1)
    b.axvspan(0, limit, color=GREEN, alpha=0.10)
    b.text(limit + 0.3, 0.03, f"error < 0.5 m in 3 s\nup to {limit:.1f} m/s² ({limit / 9.81:.2f} g)",
           color=DARK, fontsize=10)
    b.set_xlabel("lateral acceleration (m/s²)")
    b.set_ylabel("position error after 3 s (m)")
    b.set_title("Where the kinematic model is valid")
    b.grid(alpha=0.25, which="both")
    fig.tight_layout()
    fig.savefig(DOCS / "validity.png", dpi=130)
    plt.close(fig)
    return dict(zip(np.round(ay, 1), np.round(err, 2)))


def prediction_scenario(vx=10.0, dt=0.01, t0=1.5, horizon=3.0):
    """Dynamic 'truth' entering a left bend; predict from t0 with the three models."""
    t = np.arange(0, t0 + horizon + 1e-9, dt)[:-1]
    steer = np.radians(6) * np.clip((t - 1.0) / 2.0, 0, 1)
    truth = simulate_dynamic(P, vx, steer, dt)
    k0 = round(t0 / dt)
    x, y, th, vy, r = truth[k0]
    d, rate = steer[k0], np.radians(6) / 2.0
    course = th + np.arctan2(vy, vx)
    speed = np.hypot(vx, vy)
    preds = {
        "CV": predict_cv(x, y, course, speed, horizon, dt),
        "CTRV": predict_ctrv(x, y, course, speed, r, horizon, dt),
        "Bicycle": predict_bicycle(x, y, th, speed, d, horizon, dt, steer_rate=rate),
    }
    future = truth[k0:, :2]
    return truth, k0, preds, future, (x, y, th, speed, d)


def fig_prediction():
    truth, k0, preds, future, (x, y, th, v, d) = prediction_scenario()
    cone = predict_cone(x, y, th, v, d, 3.0, 0.05, steer_rate_mean=np.radians(3), samples=200)
    fig, (a, b) = plt.subplots(1, 2, figsize=(14, 5.6), gridspec_kw={"width_ratios": [1.2, 1]})
    for path in cone[::2]:
        a.plot(path[:, 0], path[:, 1], color=PURPLE, alpha=0.06, lw=1)
    a.plot([], [], color=PURPLE, alpha=0.4, lw=4, label="bicycle cone (200 samples)")
    a.plot(truth[:k0 + 1, 0], truth[:k0 + 1, 1], color=DARK, lw=2, label="driven so far")
    a.plot(future[:, 0], future[:, 1], color=GREY, lw=5, label="what really happens", zorder=1)
    styles = {"CV": (BLUE, ":"), "CTRV": (ORANGE, "--"), "Bicycle": (GREEN, "-")}
    errors = {}
    for name, p in preds.items():
        col, ls = styles[name]
        n = min(len(p), len(future))
        errors[name] = np.hypot(*(p[:n] - future[:n]).T)
        a.plot(p[:, 0], p[:, 1], color=col, ls=ls, lw=2, label=f"{name}: {errors[name][-1]:.2f} m at 3 s")
    car(a, x - G.lr * np.cos(th), y - G.lr * np.sin(th), th, d)
    a.set_aspect("equal")
    a.set_title("Entering a bend at 10 m/s: predict the next 3 s")
    a.set_xlabel("x (m)")
    a.set_ylabel("y (m)")
    a.legend(loc="upper left", fontsize=9, frameon=False)
    a.grid(alpha=0.25)

    tt = np.arange(len(errors["CV"])) * 0.01
    for name, e in errors.items():
        col, ls = styles[name]
        b.plot(tt, e, color=col, ls=ls, lw=2)
        b.text(tt[-1] + 0.05, e[-1], name, color=DARK, va="center", fontsize=10)
    b.set_xlim(0, 3.45)
    b.set_xlabel("prediction horizon (s)")
    b.set_ylabel("position error (m)")
    b.set_title("Prediction error grows with the horizon")
    b.grid(alpha=0.25)
    fig.tight_layout()
    fig.savefig(DOCS / "prediction.png", dpi=130)
    plt.close(fig)
    return {k: (round(float(np.interp(1.0, tt, e)), 2), round(float(e[-1]), 2)) for k, e in errors.items()}


def gif_figure_eight():
    v, w, dt, d0 = M.figure_eight(8.0)
    s = KinematicBicycle(G, "rear", (0, 0, 0, d0)).run(v, w, dt)
    frames = []
    for k in range(0, len(s), 40):
        fig, ax = plt.subplots(figsize=(4.6, 6.2), dpi=80)
        ax.plot(s[:, 0], s[:, 1], color=GREY, lw=1, ls="--")
        ax.plot(s[:k + 1, 0], s[:k + 1, 1], color=GREEN, lw=2.2)
        car(ax, s[k, 0], s[k, 1], s[k, 2], s[k, 3])
        ax.set_xlim(-12, 12)
        ax.set_ylim(-19, 19)
        ax.set_aspect("equal")
        ax.set_title(f"steering {np.degrees(s[k, 3]):+5.1f}°", fontsize=11, family="monospace")
        ax.axis("off")
        buf = io.BytesIO()
        fig.savefig(buf, format="png")
        plt.close(fig)
        buf.seek(0)
        frames.append(Image.open(buf).convert("P", palette=Image.ADAPTIVE, colors=64))
    frames[0].save(DOCS / "figure_eight.gif", save_all=True, append_images=frames[1:], duration=60, loop=0,
                   optimize=True)


if __name__ == "__main__":
    DOCS.mkdir(exist_ok=True)
    fig_reference_points()
    fig_maneuvers()
    print("validity (a_y: error at 3 s):", fig_validity())
    print("prediction (error at 1 s, 3 s):", fig_prediction())
    gif_figure_eight()
