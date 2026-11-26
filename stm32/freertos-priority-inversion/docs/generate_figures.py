"""
Regenerate the README figures:  python docs/generate_figures.py
Requires: matplotlib.

All data comes from tools/rtos_sim.py, a model of the FreeRTOS scheduling rules
(including priority inheritance) running the firmware's scene. On the board the
same blocking times come from the `report` command.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import rtos_sim as rs
import scenarios as sc


def timelines():
    fig, axes = plt.subplots(2, 1, figsize=(11, 5.6), dpi=150, sharex=True)
    for ax, kind in zip(axes, ("semaphore", "mutex"), strict=True):
        r = sc.simulate(kind, 60)
        rs.plot_timeline(ax, r, sc.ORDER, sc.COLORS, show_prio_change=True)
        wait = sc.wait_per_cycle_ms(r)[0]
        req = next(t for t, task, what in r.marks if task == "high" and what == "request")
        acq = next(t for t, task, what in r.marks if task == "high" and what == "acquired")
        ax.annotate("", xy=(acq, 2.45), xytext=(req, 2.45),
                    arrowprops={"arrowstyle": "<->", "color": "#c53030", "lw": 1.4})
        ax.text((req + acq) / 2, 2.55, f"high blocked {wait:.0f} ms", ha="center", fontsize=9, color="#c53030")
        title = ("Binary semaphore: medium, which needs no lock at all, delays high (priority inversion)"
                 if kind == "semaphore" else
                 "Mutex: low inherits high's priority (hatched), medium must wait, high is delayed only by low")
        ax.set_title(title, fontsize=10, loc="left")
        ax.set_ylim(-0.6, 2.9)
    axes[-1].set_xlabel("time in the 100 ms cycle (ms): low takes the lock at 0, high asks at 2, medium wakes at 3")
    fig.tight_layout()
    fig.savefig(HERE / "inversion_timelines.png", facecolor="white")
    plt.close(fig)


def wait_vs_medium():
    mediums = list(range(0, 61, 5))
    sem = [sc.wait_per_cycle_ms(sc.simulate("semaphore", 100, medium_ms=m))[0] for m in mediums]
    mtx = [sc.wait_per_cycle_ms(sc.simulate("mutex", 100, medium_ms=m))[0] for m in mediums]
    fig, ax = plt.subplots(figsize=(8, 3.8), dpi=150)
    ax.plot(mediums, sem, "o-", color="#c53030", label="binary semaphore: unbounded")
    ax.plot(mediums, mtx, "s-", color="#2f855a", label="mutex (priority inheritance): bounded")
    ax.set_xlabel("CPU time of the medium task per cycle (ms)")
    ax.set_ylabel("high task blocked (ms)")
    ax.set_title("How long the most important task waits (low holds the lock for 10 ms)", fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=9)
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
    fig.tight_layout()
    fig.savefig(HERE / "blocking_vs_medium.png", facecolor="white")
    plt.close(fig)


if __name__ == "__main__":
    timelines()
    wait_vs_medium()
    print("figures written to", HERE)
