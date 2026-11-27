"""Regenerate the README figures:  python tools/make_figures.py

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import logging
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from esekf import SensorSpec, make_truth, rmse, run_filter, simulate_sensors

DOCS = ROOT / "docs"
BLUE, ORANGE, GREY, GREEN, RED, DARK = "#2b6cb0", "#dd6b20", "#a0aec0", "#2f855a", "#c53030", "#2d3748"
logging.getLogger("matplotlib.font_manager").setLevel(logging.ERROR)   # log-axis labels fall back quietly
plt.rcParams.update({"font.size": 11, "axes.spines.top": False, "axes.spines.right": False})
CHI2_9 = (2.700, 19.023)     # 2.5 % and 97.5 % quantiles of chi-square with 9 degrees of freedom


def main():
    DOCS.mkdir(exist_ok=True)
    truth = make_truth(200.0)
    meas = simulate_sensors(truth)
    fused = run_filter(truth, meas)
    gnss_only = run_filter(truth, meas, use_lidar=False)
    imu_only = run_filter(truth, meas, use_gnss=False, use_lidar=False)
    t = truth.t

    # ---- 1. the drive --------------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(12, 6.2))
    ax.plot(meas.gnss_p[:, 0], meas.gnss_p[:, 1], ".", color=GREY, ms=3, label="GNSS fixes (σ = 1 m)")
    ax.plot(imu_only.p[:, 0], imu_only.p[:, 1], color=RED, lw=1.2, label="IMU only (dead reckoning)")
    ax.plot(truth.p[:, 0], truth.p[:, 1], color=DARK, lw=3, label="ground truth")
    ax.plot(fused.p[:, 0], fused.p[:, 1], color=BLUE, lw=1.3,
            label=f"ES-EKF, IMU + GNSS + LiDAR (RMSE {100 * rmse(fused.err[:, 0:3]):.0f} cm)")
    ax.set_xlim(-190, 190)
    ax.set_ylim(-120, 120)
    ax.set_aspect("equal")
    ax.set_xlabel("east (m)")
    ax.set_ylabel("north (m)")
    ax.legend(frameon=False, loc="upper center", ncol=2, fontsize=10)
    ax.set_title("Two laps of a 1 km figure eight (200 s): the IMU alone leaves the map within a minute")
    fig.tight_layout()
    fig.savefig(DOCS / "trajectory.png", dpi=130)
    plt.close(fig)

    # ---- 2. errors and 3-sigma bounds ----------------------------------------------------------------
    fig, axes = plt.subplots(3, 3, figsize=(14, 8), sharex=True)
    names = ["east", "north", "up"]
    groups = [("position (m)", 0, 1.0), ("velocity (m/s)", 3, 1.0), ("attitude (deg)", 6, 180 / np.pi)]
    att_names = ["roll", "pitch", "yaw"]
    for row, (label, off, scale) in enumerate(groups):
        for c in range(3):
            ax = axes[row, c]
            s3 = 3 * fused.sigma[:, off + c] * scale
            ax.fill_between(t, -s3, s3, color=BLUE, alpha=0.15, lw=0)
            ax.plot(t, fused.err[:, off + c] * scale, color=BLUE, lw=0.8)
            ax.set_title(f"{label.split(' ')[0]}: {(att_names if row == 2 else names)[c]}", fontsize=10)
            if c == 0:
                ax.set_ylabel(label)
            ax.grid(alpha=0.3)
    for ax in axes[2]:
        ax.set_xlabel("time (s)")
    fig.suptitle("Estimation error (line) inside the filter's own ±3σ bound (band): the filter knows how good it is")
    fig.tight_layout()
    fig.savefig(DOCS / "errors_3sigma.png", dpi=130)
    plt.close(fig)

    # ---- 3. outage ------------------------------------------------------------------------------------
    spec = SensorSpec(gnss_outage=(80, 110), lidar_outage=(80, 110))
    out = run_filter(truth, simulate_sensors(truth, spec))
    e = np.linalg.norm(out.err[:, 0:3], axis=1)
    s3 = 3 * np.linalg.norm(out.sigma[:, 0:3], axis=1)
    fig, axes = plt.subplots(1, 2, figsize=(14, 4.6))
    ax = axes[0]
    ax.axvspan(80, 110, color=ORANGE, alpha=0.12, lw=0)
    ax.plot(t, s3, "--", color=GREY, label="3σ bound from the filter")
    ax.plot(t, e, color=BLUE, lw=1.2, label="position error")
    ax.text(113, 8, "GNSS and LiDAR lost\nfor 30 s (tunnel)", color=ORANGE)
    ax.set_ylim(0.01, 60)
    ax.set_yscale("log")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("m")
    ax.legend(frameon=False, loc="upper right")
    ax.set_title(f"Outage: drift to {e[(110 * 200) - 1]:.1f} m, back to {100 * e[(111 * 200)]:.0f} cm "
                 "1 s after the fixes return", fontsize=11)
    ax.grid(alpha=0.3)
    ax = axes[1]
    for r, col, lab in ((imu_only, RED, "IMU only"), (gnss_only, ORANGE, "IMU + GNSS"),
                        (fused, BLUE, "IMU + GNSS + LiDAR")):
        ax.plot(t, np.linalg.norm(r.err[:, 0:3], axis=1), color=col, lw=1,
                label=f"{lab}: RMSE {rmse(r.err[:, 0:3]):.2f} m" if lab != "IMU only"
                else f"{lab}: {np.linalg.norm(r.err[-1, 0:3]) / 1000:.1f} km after 200 s")
    ax.set_yscale("log")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("position error (m)")
    ax.set_ylim(0.01, 3e4)
    ax.legend(frameon=False, loc="center right")
    ax.set_title("Each sensor added", fontsize=11)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(DOCS / "outage_and_sensors.png", dpi=130)
    plt.close(fig)

    # ---- 4. bias estimation ------------------------------------------------------------------------------
    fig, axes = plt.subplots(1, 2, figsize=(14, 4.4))
    cols = [BLUE, ORANGE, GREEN]
    for ax, est, off, true, scale, unit, name in (
            (axes[0], fused.ba, 9, meas.spec.acc_bias, 1000, "mm/s²", "accelerometer bias"),
            (axes[1], fused.bg, 12, meas.spec.gyro_bias, 180 / np.pi, "deg/s", "gyroscope bias")):
        for c in range(3):
            s = 3 * fused.sigma[:, off + c] * scale
            ax.fill_between(t, est[:, c] * scale - s, est[:, c] * scale + s, color=cols[c], alpha=0.12, lw=0)
            ax.plot(t, est[:, c] * scale, color=cols[c], lw=1.3, label=f"{'xyz'[c]}")
            ax.axhline(true[c] * scale, color=cols[c], ls=":", lw=1.5)
        ax.set_xlabel("time (s)")
        ax.set_ylabel(unit)
        ax.set_title(f"{name}: estimate (line, ±3σ) converges to the true value (dotted)", fontsize=11)
        ax.legend(frameon=False, ncol=3, loc="lower right")
        ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(DOCS / "bias_estimation.png", dpi=130)
    plt.close(fig)

    # ---- 5. consistency: NEES catches a wrong LiDAR calibration and missing bias states --------------------
    wrong = run_filter(truth, meas, lidar_rpy_assumed=(0.05, 0.05, 0.05))
    nobias = run_filter(truth, meas, estimate_biases=False)
    fig, ax = plt.subplots(figsize=(14, 4.6))
    w = 200                                    # 1-s moving average
    smooth = lambda x: np.convolve(x, np.ones(w) / w, mode="same")
    ax.axhspan(*CHI2_9, color=GREEN, alpha=0.12, lw=0, label="95 % band for a consistent filter (χ², 9 dof)")
    ax.plot(t, smooth(fused.nees), color=BLUE, lw=1.3, label=f"correct model: mean NEES {fused.nees.mean():.1f}")
    ax.plot(t, smooth(nobias.nees), color=ORANGE, lw=1.3,
            label=f"biases not estimated: mean {nobias.nees.mean():.0f}, RMSE {rmse(nobias.err[:, 0:3]):.1f} m")
    ax.plot(t, smooth(wrong.nees), color=RED, lw=1.3,
            label=f"LiDAR yaw calibration off by 0.05 rad: mean {wrong.nees.mean():.0f}, "
                  f"RMSE {rmse(wrong.err[:, 0:3]):.1f} m")
    ax.set_yscale("log")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("NEES (1-s average)")
    ax.legend(frameon=False, loc="upper left", fontsize=10)
    ax.set_title("Consistency check: a model error shows up as NEES far above its χ² band")
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(DOCS / "consistency.png", dpi=130)
    plt.close(fig)

    print(f"fused RMSE {rmse(fused.err[:, 0:3]):.3f} m, gnss-only {rmse(gnss_only.err[:, 0:3]):.3f} m, "
          f"imu-only final {np.linalg.norm(imu_only.err[-1, 0:3]):.0f} m, NEES {fused.nees.mean():.2f}, "
          f"wrong-calib {rmse(wrong.err[:, 0:3]):.2f} m / NEES {wrong.nees.mean():.0f}, "
          f"no-bias {rmse(nobias.err[:, 0:3]):.2f} m / NEES {nobias.nees.mean():.0f}, "
          f"att RMSE {np.degrees(rmse(fused.err[:, 6:9])):.2f} deg, "
          f"outage drift {e[(110 * 200) - 1]:.2f} m")
    print("figures written to", DOCS)


if __name__ == "__main__":
    main()
