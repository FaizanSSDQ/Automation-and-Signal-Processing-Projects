"""
Regenerate the README figures:  python docs/generate_figures.py
Requires: matplotlib, numpy.

All data comes from tools/rtos_sim.py, a model of the FreeRTOS scheduling rules
running the firmware's task set (2 ms work per cycle, 3 ms interference burst
every 37 ms). On the board the same numbers come from the `report` command.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import rtos_sim as rs
import scenarios as sc


def clean(ax):
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
    ax.grid(True, alpha=0.3)


def timeline():
    r = sc.simulate(80)
    fig, ax = plt.subplots(figsize=(11, 3.0), dpi=150)
    rs.plot_timeline(ax, r, sc.ORDER, sc.COLORS, min_width=0.3)
    for row, task in enumerate(sc.ORDER):
        y = len(sc.ORDER) - 1 - row
        for t in sc.releases(r, task):
            ax.plot(t, y + 0.45, "v", color="black", ms=4)
    for x in np.arange(0, 81, 10):
        ax.axvline(x, color="#2b6cb0", lw=0.6, ls=":")
    ax.set_xlabel("time (ms)   (dotted: the ideal 10 ms grid, ▼: releases)")
    ax.set_title("First 80 ms: 'relative' slips further behind the grid every cycle; 'absolute' stays on it",
                 fontsize=10, loc="left")
    fig.tight_layout()
    fig.savefig(HERE / "timeline.png", facecolor="white")
    plt.close(fig)


def drift():
    r = sc.simulate(2000)
    fig, ax = plt.subplots(figsize=(10, 3.8), dpi=150)
    for task, label in (("relative", "relative (vTaskDelay)"), ("absolute", "absolute (vTaskDelayUntil)")):
        rel = sc.releases(r, task)
        late = sc.lateness(r, task)
        ax.plot(np.array(rel) / 1000, late, color=sc.COLORS[task], lw=1.8, label=label)
    ax.set_xlabel("time (s)")
    ax.set_ylabel("release time − ideal (ms)")
    n_rel = len(sc.releases(r, "relative"))
    n_abs = len(sc.releases(r, "absolute"))
    ax.set_title(f"Accumulated timing error: in 2 s, relative ran {n_rel} cycles and absolute {n_abs} "
                 "(ideal: 200)", fontsize=10)
    ax.legend(fontsize=9)
    clean(ax)
    fig.tight_layout()
    fig.savefig(HERE / "drift.png", facecolor="white")
    plt.close(fig)


def histograms():
    r = sc.simulate(10000)
    fig, axes = plt.subplots(1, 2, figsize=(11, 3.6), dpi=150, sharey=True)
    bins = np.arange(5.5, 18.6, 0.5)
    for ax, task in zip(axes, ("relative", "absolute"), strict=True):
        p = np.array(sc.periods(r, task))
        ax.hist(p, bins=bins, color=sc.COLORS[task], edgecolor="white")
        ax.axvline(10, color="black", ls="--", lw=1)
        ax.set_title(f"{task}: mean {p.mean():.2f} ms, jitter (sd) {p.std():.2f} ms, range "
                     f"{p.min():.0f}–{p.max():.0f} ms", fontsize=9.5)
        ax.set_xlabel("period (ms)")
        clean(ax)
    axes[0].set_ylabel("cycles in 10 s")
    fig.suptitle("Period distribution (dashed: nominal 10 ms)", fontsize=11)
    fig.tight_layout()
    fig.savefig(HERE / "period_histograms.png", facecolor="white")
    plt.close(fig)


if __name__ == "__main__":
    timeline()
    drift()
    histograms()
    print("figures written to", HERE)
