"""
Regenerate the README figures:  make sim && python docs/generate_figures.py
Requires: numpy, matplotlib.

The captures are produced by the host simulator, i.e. the firmware's own
SUMP parser, trigger and capture engine running on modeled demo signals.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyArrowPatch, Rectangle

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import sump_capture as sc

BLUE, ORANGE, GREEN, GREY, RED, PURPLE = "#2b6cb0", "#dd6b20", "#2f855a", "#4a5568", "#c53030", "#6b46c1"
CH_COLORS = [BLUE, ORANGE, GREEN, PURPLE]
CH_NAMES = ["CH0  PC0 ← PA6  10 kHz 50 %", "CH1  PC1 ← PA7  10 kHz 25 %",
            "CH2  PC2 ← PB6  1 kHz 10 %", "CH3  PC3 ← PA9  UART 9600"]


def simulated_capture(rate, samples, post, trigger):
    port = sc.SimPort()
    try:
        la = sc.Analyzer(port)
        la.reset()
        divider = sc.divider_for(rate)
        data = la.capture(divider, samples, post, sc.parse_trigger(trigger))
        return np.array(data, dtype=np.uint8), sc.actual_rate(divider)
    finally:
        port.close()


def draw_channels(ax, data, rate, trig_index, unit_scale, unit):
    t = (np.arange(len(data) + 1) - trig_index) / rate * unit_scale
    for row in range(4):
        bits = (data >> row) & 1
        y = 3 - row
        ax.stairs(bits * 0.65 + y * 1.0, t, baseline=None, color=CH_COLORS[row], lw=1.2)
        ax.text(t[0] - (t[-1] - t[0]) * 0.01, y + 0.32, CH_NAMES[row], ha="right", va="center", fontsize=8.5)
    ax.axvline(0, color=RED, ls="--", lw=1)
    ax.text(0, 3.95, " trigger", color=RED, fontsize=8, va="bottom")
    ax.set_xlim(t[0], t[-1])
    ax.set_ylim(-0.3, 4.3)
    ax.set_yticks([])
    ax.set_xlabel(f"time relative to trigger ({unit})")
    for side in ("left", "right", "top"):
        ax.spines[side].set_visible(False)
    return t


def demo_capture():
    """Two captures of the demo outputs: a UART-triggered overview and a PWM zoom."""
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(11, 6.4), dpi=150)

    pre = 164
    data, rate = simulated_capture(100e3, 4096, 4096 - pre, "3:f")
    draw_channels(a1, data, rate, pre, 1e3, "ms")
    for start, byte in sc.decode_uart(data.tolist(), 3, rate, 9600):
        centre = (start - pre + 5 * rate / 9600) / rate * 1e3
        label = {13: "\\r", 10: "\\n", 32: "␣"}.get(byte, chr(byte))
        a1.text(centre, 0.70, label, ha="center", va="bottom", fontsize=7.5, color=PURPLE, family="monospace")
    a1.set_title("100 kS/s, 4096 samples, trigger: CH3 falling edge (UART start bit), 4 % pre-trigger\n"
                 "decoded: \"LA demo 0000\\r\\n\"", fontsize=10)

    pre = 256
    data, rate = simulated_capture(4e6, 1024, 1024 - pre, "0:r")
    draw_channels(a2, data, rate, pre, 1e6, "µs")
    a2.set_title("4 MS/s, 1024 samples, trigger: CH0 rising edge, 25 % pre-trigger", fontsize=10)

    fig.tight_layout()
    fig.savefig(HERE / "demo_capture.png", facecolor="white")
    plt.close(fig)


def capture_timeline():
    """How the ring, the trigger scan and the pre/post-trigger window relate."""
    fig, ax = plt.subplots(figsize=(11, 4.0), dpi=150)
    ring, depth, pre = 12288, 8192, 2048
    trig = 15000
    start, end = trig - pre, trig - pre + depth
    stop = end + 1500

    ax.add_patch(Rectangle((0, 0.55), stop, 0.3, color=GREY, alpha=0.12))
    ax.add_patch(Rectangle((stop - ring, 0.55), ring, 0.3, fill=False, ec=GREY, lw=1.2, ls="--"))
    ax.text(stop - ring / 2, 0.92, f"ring: the last {ring} samples (12 KB)", ha="center", fontsize=9, color=GREY)
    ax.add_patch(Rectangle((start, 0.55), pre, 0.3, color=ORANGE, alpha=0.55))
    ax.add_patch(Rectangle((trig, 0.55), depth - pre, 0.3, color=BLUE, alpha=0.55))
    ax.text(start + pre / 2, 0.70, "pre-trigger", ha="center", va="center", fontsize=9, color="white")
    ax.text(trig + (depth - pre) / 2, 0.70, "post-trigger", ha="center", va="center", fontsize=9, color="white")
    ax.add_patch(Rectangle((end, 0.55), stop - end, 0.3, color=GREEN, alpha=0.35))

    marks = [(0, "arm\n(TIM2 + DMA start)", GREY), (pre, "trigger allowed\n(pre samples exist)", GREY),
             (trig, "trigger found\nby the scanner", RED), (end, "post-trigger complete\n→ stop TIM2", BLUE),
             (stop, "timer stopped\n(head room used)", GREEN)]
    for k, (x, text, color) in enumerate(marks):
        low = 0.40 if k % 2 == 0 else 0.16  # alternate rows so labels never collide
        ax.plot([x, x], [low + 0.03, 0.88], color=color, lw=1.2)
        ax.text(x, low, text, ha="center", va="top", fontsize=8, color=color)
    ax.add_patch(FancyArrowPatch((start, -0.20), (end, -0.20), arrowstyle="<->", mutation_scale=10))
    ax.text((start + end) / 2, -0.33, f"window sent to the host: {depth} samples, trigger at index {pre}",
            ha="center", fontsize=9)
    ax.set_xlim(-1200, stop + 1500)
    ax.set_ylim(-0.42, 1.05)
    ax.set_xlabel("sample index since arming")
    ax.set_yticks([])
    for side in ("left", "right", "top"):
        ax.spines[side].set_visible(False)
    ax.set_title("Pre-trigger capture on a circular DMA ring", fontsize=10)
    fig.tight_layout()
    fig.savefig(HERE / "capture_timeline.png", facecolor="white")
    plt.close(fig)


def scan_budget():
    """CPU cycles available per sample versus what the trigger scan needs."""
    rates = np.logspace(5, np.log10(8e6), 300)
    budget = sc.TIMER_CLOCK_HZ / rates
    fig, ax = plt.subplots(figsize=(8, 4), dpi=150)
    ax.loglog(rates, budget, color=GREY, lw=2, label="cycles per sample at 64 MHz")
    ax.axhline(10, color=BLUE, ls="--", lw=1.4, label="level-trigger scan ≈ 10 cycles/sample")
    ax.axhline(14, color=ORANGE, ls="--", lw=1.4, label="edge-trigger scan ≈ 14 cycles/sample")
    ax.axvline(4e6, color=RED, lw=1.2)
    ax.text(4e6, 300, " 4 MS/s\n firmware limit", color=RED, fontsize=9, va="top")
    ax.set_xlabel("sample rate (S/s)")
    ax.set_ylabel("CPU cycles per sample")
    ax.set_title("Trigger-scan budget (immediate captures need no scan)", fontsize=10)
    ax.grid(True, which="both", alpha=0.25)
    ax.legend(fontsize=8.5, loc="upper right")
    fig.tight_layout()
    fig.savefig(HERE / "scan_budget.png", facecolor="white")
    plt.close(fig)


if __name__ == "__main__":
    demo_capture()
    capture_timeline()
    scan_budget()
    print("figures written to", HERE)
