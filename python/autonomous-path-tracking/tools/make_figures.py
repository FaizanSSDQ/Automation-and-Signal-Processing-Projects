"""Regenerate the README figures and the animation:  python tools/make_figures.py

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import io
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
from pathtrack import (
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

DOCS = ROOT / "docs"
BLUE, ORANGE, GREY, GREEN, DARK = "#2b6cb0", "#dd6b20", "#a0aec0", "#2f855a", "#2d3748"
plt.rcParams.update({"font.size": 11, "axes.spines.top": False, "axes.spines.right": False})


def overview():
    paths = [("Lane change, 15 m/s", lane_change()), ("Figure eight", figure_eight()), ("Circuit", racetrack())]
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.8))
    for ax, (name, p) in zip(axes, paths):
        ax.plot(p.x, p.y, color=GREY, lw=7, solid_capstyle="round", label="reference path")
        for ctrl, col, lab in ((PurePursuit(), ORANGE, "pure pursuit"), (Stanley(), BLUE, "Stanley")):
            r = simulate(p, ctrl, t_max=120)
            m = tracking_metrics(r)
            ax.plot(r.x, r.y, color=col, lw=1.6, label=f"{lab}: RMS {100 * m.rms_cte:.0f} cm")
        ax.set_title(name)
        ax.set_aspect("equal" if name != "Lane change, 15 m/s" else "auto")
        ax.legend(frameon=False, fontsize=9, loc="best")
        ax.set_xlabel("x (m)")
    axes[0].set_ylabel("y (m)")
    fig.tight_layout()
    fig.savefig(DOCS / "overview.png", dpi=130)
    plt.close(fig)


def errors():
    p = racetrack()
    fig, axes = plt.subplots(3, 1, figsize=(12, 7.5), sharex=True)
    for ctrl, col, lab in ((PurePursuit(), ORANGE, "pure pursuit"), (Stanley(), BLUE, "Stanley")):
        r = simulate(p, ctrl, t_max=120)
        axes[0].plot(r.t, 100 * r.cte, color=col, lw=1.3, label=lab)
        axes[1].plot(r.t, np.degrees(r.steer), color=col, lw=1.3)
        axes[2].plot(r.t, r.v, color=col, lw=1.3)
    rr = simulate(p, Stanley(), t_max=120)
    axes[2].plot(rr.t, p.v_ref[rr.idx], "--", color=DARK, lw=1, label="speed profile (lateral-g limited)")
    axes[0].set_ylabel("cross-track\nerror (cm)")
    axes[1].set_ylabel("steering (deg)")
    axes[2].set_ylabel("speed (m/s)")
    axes[2].set_xlabel("time (s)")
    axes[0].legend(frameon=False, ncol=2)
    axes[2].legend(frameon=False, loc="lower right")
    axes[0].set_title("One lap of the circuit: errors peak in the hairpin and the chicane")
    for ax in axes:
        ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(DOCS / "circuit_errors.png", dpi=130)
    plt.close(fig)


def recovery_and_tuning():
    fig, axes = plt.subplots(1, 2, figsize=(14, 4.6))
    p = lane_change(length=150, offset=0.0)
    start = VehicleState(0.0, 2.0, 0.0, 15.0, 0.0)
    for ctrl, col, lab in ((PurePursuit(), ORANGE, "pure pursuit"), (Stanley(), BLUE, "Stanley")):
        r = simulate(p, ctrl, start=start, t_max=8)
        axes[0].plot(r.t, r.cte, color=col, lw=1.8, label=lab)
    axes[0].axhline(0, color=GREY, lw=1)
    axes[0].set_xlabel("time (s)")
    axes[0].set_ylabel("cross-track error (m)")
    axes[0].set_title("Starting 2 m off the lane at 15 m/s")
    axes[0].legend(frameon=False)
    axes[0].grid(alpha=0.3)

    p = figure_eight()
    gains = [0.2, 0.3, 0.45, 0.6, 0.9, 1.2, 1.6]
    cte, rate = [], []
    for g in gains:
        m = tracking_metrics(simulate(p, PurePursuit(gain=g), t_max=120))
        cte.append(100 * m.rms_cte)
        rate.append(m.max_steer_rate)
    ax = axes[1]
    ax.plot(gains, cte, "o-", color=ORANGE, label="RMS cross-track error (cm)")
    ax.set_xlabel("pure pursuit look-ahead gain (s)")
    ax.set_ylabel("RMS error (cm)", color=ORANGE)
    ax2 = ax.twinx()
    ax2.plot(gains, rate, "s--", color=GREEN, label="peak steering rate (deg/s)")
    ax2.set_ylabel("peak steering rate (deg/s)", color=GREEN)
    ax2.spines["right"].set_visible(True)
    ax.set_title("Look-ahead on the figure eight: error grows with it, steering is calmest near 0.45 s")
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(DOCS / "recovery_tuning.png", dpi=130)
    plt.close(fig)


def car_polygon(x, y, yaw, steer, L=2.7, scale=3.0):
    """Body outline and front wheels, in world coordinates (drawn `scale` times life size)."""
    c, s = np.cos(yaw), np.sin(yaw)
    body = np.array([[-0.9, -0.9], [L + 0.9, -0.9], [L + 0.9, 0.9], [-0.9, 0.9]])
    rot = np.array([[c, -s], [s, c]])
    body_w = scale * body @ rot.T + [x, y]
    wheels = []
    for wy in (-0.8, 0.8):
        w = np.array([[-0.35, -0.12], [0.35, -0.12], [0.35, 0.12], [-0.35, 0.12]])
        cs, ss = np.cos(steer), np.sin(steer)
        w = w @ np.array([[cs, -ss], [ss, cs]]).T + [L, wy]
        wheels.append(scale * w @ rot.T + [x, y])
    return body_w, wheels


def animation():
    p = racetrack()
    r = simulate(p, Stanley(), t_max=120)
    frames = []
    step = 25                                    # 0.25 s per frame
    for k in range(0, len(r.t), step):
        fig, ax = plt.subplots(figsize=(6.4, 4.0), dpi=90)
        ax.plot(p.x, p.y, color=GREY, lw=9, solid_capstyle="round")
        ax.plot(p.x, p.y, color="white", lw=0.8, ls=(0, (6, 6)))
        ax.plot(r.x[: k + 1], r.y[: k + 1], color=BLUE, lw=1.8)
        body, wheels = car_polygon(r.x[k], r.y[k], r.yaw[k], r.steer[k])
        ax.add_patch(Polygon(body, closed=True, color=ORANGE, zorder=5))
        for w in wheels:
            ax.add_patch(Polygon(w, closed=True, color=DARK, zorder=6))
        ax.text(0.02, 0.95, f"Stanley   t = {r.t[k]:4.1f} s   v = {r.v[k]:4.1f} m/s   e = {100 * r.cte[k]:+5.1f} cm   (car 3x scale)",
                transform=ax.transAxes, fontsize=9, family="monospace", va="top")
        ax.set_aspect("equal")
        ax.set_xlim(-25, 120)
        ax.set_ylim(-10, 72)
        ax.axis("off")
        fig.tight_layout(pad=0.2)
        buf = io.BytesIO()
        fig.savefig(buf, format="png")
        plt.close(fig)
        buf.seek(0)
        frames.append(Image.open(buf).convert("P", palette=Image.ADAPTIVE, colors=64))
    frames[0].save(DOCS / "stanley_circuit.gif", save_all=True, append_images=frames[1:], duration=80, loop=0,
                   optimize=True)


if __name__ == "__main__":
    DOCS.mkdir(exist_ok=True)
    VehicleParams()   # documents the defaults used everywhere
    overview()
    errors()
    recovery_and_tuning()
    animation()
    print("figures written to", DOCS)
