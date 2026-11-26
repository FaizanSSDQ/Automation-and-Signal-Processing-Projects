"""
Regenerate the README figures:  python docs/generate_figures.py
Requires: matplotlib.

The timelines come from tools/rtos_sim.py, a model of the FreeRTOS scheduling
rules running the same task set as the firmware. On the board, the same
picture is captured live from the trace pins with a logic analyzer.

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


def scenario_timelines():
    names = ["normal", "preemption", "starvation", "time_slicing"]
    fig, axes = plt.subplots(len(names), 1, figsize=(11, 9.2), dpi=150, sharex=True)
    for ax, name in zip(axes, names, strict=True):
        r = sc.simulate(name, 300)
        rs.plot_timeline(ax, r, sc.ORDER, sc.COLORS, title=sc.SCENARIOS[name]["title"], min_width=1.2)
        lat = sc.toggle_latencies_ms(r, "blink_fast")
        text = f"blink_fast wake-to-run: max {max(lat):.1f} ms" if lat else "blink_fast never runs"
        ax.text(0.995, 0.04, text, transform=ax.transAxes, ha="right", va="bottom", fontsize=8.5,
                color="#4a5568", bbox={"boxstyle": "round,pad=0.25", "fc": "white", "ec": "#cbd5e0"})
        for t, task, what in r.marks:
            if task == "console" and what == "command":
                ax.annotate("command\narrives", (t, 3.35), xytext=(t + 8, 3.55), fontsize=7.5,
                            color="#6b46c1", arrowprops={"arrowstyle": "->", "color": "#6b46c1", "lw": 0.8})
    axes[-1].set_xlabel("time (ms)")
    fig.suptitle("Who owns the CPU: the firmware's task set in four configurations (simulated)", fontsize=11)
    fig.tight_layout()
    fig.savefig(HERE / "scheduling_timelines.png", facecolor="white")
    plt.close(fig)


def latency_chart():
    names = ["normal", "preemption", "time_slicing"]
    labels = ["worker 'below'\n30 % load", "worker 'below'\n70 % load", "worker 'low'\n100 % load"]
    worst = []
    mean = []
    for n in names:
        lat = sc.toggle_latencies_ms(sc.simulate(n, 2000), "blink_fast")
        worst.append(max(lat))
        mean.append(sum(lat) / len(lat))
    fig, ax = plt.subplots(figsize=(7.5, 3.6), dpi=150)
    x = range(len(names))
    ax.bar([i - 0.18 for i in x], worst, width=0.36, color="#dd6b20", label="worst case")
    ax.bar([i + 0.18 for i in x], mean, width=0.36, color="#2b6cb0", label="average")
    for i in x:
        ax.text(i - 0.18, worst[i] + 1, f"{worst[i]:.1f}", ha="center", fontsize=8.5)
        ax.text(i + 0.18, mean[i] + 1, f"{mean[i]:.1f}", ha="center", fontsize=8.5)
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, fontsize=9)
    ax.set_ylabel("ms")
    ax.set_title("blink_fast (low priority): delay from wake-up to running", fontsize=10)
    ax.legend(fontsize=8.5)
    ax.grid(True, axis="y", alpha=0.3)
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
    fig.tight_layout()
    fig.savefig(HERE / "wake_latency.png", facecolor="white")
    plt.close(fig)


if __name__ == "__main__":
    scenario_timelines()
    latency_chart()
    print("figures written to", HERE)
