"""
Regenerate the README figures:  python docs/generate_figures.py
Requires: matplotlib, schemdraw.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import schemdraw  # noqa: E402
import schemdraw.elements as elm  # noqa: E402

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
from adc_viewer import (  # noqa: E402
    TYPE_SAMPLES,
    TYPE_STATS,
    FrameParser,
    counts_to_mv,
    decode_samples,
    decode_stats,
    simulate_stream,
)

BLOCK_MS = 256 / 2000 * 1000  # 128 ms
UART_MS = 1070 * 10 / 230400 * 1000  # stats + samples frames, 10 bits per byte
BLUE, ORANGE, GREEN, GREY = "#2b6cb0", "#dd6b20", "#2f855a", "#4a5568"


def timeline() -> None:
    fig, ax = plt.subplots(figsize=(11, 3.4), dpi=150)
    rows = {"DMA → buffer": 2, "CPU (main loop)": 1, "UART TX (DMA)": 0}
    for i in range(4):
        t0 = i * BLOCK_MS
        half = "A" if i % 2 == 0 else "B"
        color = BLUE if half == "A" else ORANGE
        ax.broken_barh([(t0, BLOCK_MS)], (2 - 0.3, 0.6), facecolors=color, alpha=0.85)
        ax.text(t0 + BLOCK_MS / 2, 2, f"filling half {half}", ha="center", va="center",
                color="white", fontsize=9, weight="bold")
        irq = "HT IRQ" if half == "A" else "TC IRQ"
        ax.annotate(irq, xy=(t0 + BLOCK_MS, 2.32), xytext=(t0 + BLOCK_MS, 2.75), ha="center",
                    fontsize=8, color=GREY, arrowprops={"arrowstyle": "->", "color": GREY})
        if i < 3:
            p0 = t0 + BLOCK_MS
            ax.broken_barh([(p0, 4)], (1 - 0.3, 0.6), facecolors=GREEN)
            ax.text(p0 + 6, 1, f"stats + frame half {half}", va="center", fontsize=8)
            ax.broken_barh([(p0 + 4, UART_MS)], (0 - 0.3, 0.6), facecolors=color, alpha=0.45,
                           edgecolor=color)
            ax.text(p0 + 4 + UART_MS / 2, 0, f"{UART_MS:.0f} ms", ha="center", va="center",
                    fontsize=8)
    ax.set_yticks(list(rows.values()), list(rows.keys()))
    ax.set_xlim(0, 4 * BLOCK_MS)
    ax.set_ylim(-0.6, 3.0)
    ax.set_xlabel("time (ms)  —  one block = 256 frames ÷ 2 kHz = 128 ms")
    ax.set_title("Ping-pong DMA: the CPU always works on the half the DMA is NOT writing",
                 fontsize=11)
    ax.grid(axis="x", alpha=0.25)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    fig.tight_layout()
    fig.savefig(HERE / "pingpong_timeline.png")
    plt.close(fig)


def front_end() -> None:
    d = schemdraw.Drawing(show=False)
    d.config(fontsize=11, unit=2.6, bgcolor="white", margin=0.6)
    d.add(elm.Dot(open=True).label("Signal in\n(AC, ±1.5 V)", "left"))
    d.add(elm.Line().right().length(0.6))
    d.add(elm.Capacitor().right().label("C1  1 µF"))
    bias = d.add(elm.Dot())
    d.add(elm.Resistor().right().label("R3  1.5 kΩ"))
    adc = d.add(elm.Dot())
    d.add(elm.Line().right().length(1.5))
    d.add(elm.Dot(open=True).label("PA1 / A1\n(ADC_IN1)", "right"))
    d.add(elm.Resistor().up().at(bias.center).label("R1\n10 kΩ", loc="bottom"))
    d.add(elm.Vdd().label("3.3 V"))
    d.add(elm.Resistor().down().at(bias.center).label("R2\n10 kΩ", loc="top"))
    d.add(elm.Ground())
    d.add(elm.Capacitor().down().at(adc.center).label("C2\n100 nF", loc="top"))
    d.add(elm.Ground())
    d.add(elm.Label().at((adc.center[0] - 1.2, adc.center[1] - 4.3)).label(
        "Bias R1/R2 centres the signal at 1.65 V (mid-scale).\n"
        "R3·C2 anti-aliasing low-pass: fc = 1 / (2π · 1.5 kΩ · 100 nF) ≈ 1.06 kHz (fs = 2 kHz)",
        fontsize=10))
    d.save(str(HERE / "analog_front_end.svg"))


def viewer_snapshot() -> None:
    parser = FrameParser()
    ch = [[], []]
    stats = None
    stream = simulate_stream()
    for _ in range(4):
        for frame in parser.feed(next(stream)):
            if frame.type == TYPE_SAMPLES:
                for c, values in enumerate(decode_samples(frame.payload)):
                    ch[c] += [counts_to_mv(v) for v in values]
            elif frame.type == TYPE_STATS:
                stats = decode_stats(frame.payload)

    t_ms = [i / 2 for i in range(len(ch[0]))]  # 2 kHz -> 0.5 ms per sample
    fig, ax = plt.subplots(figsize=(11, 4), dpi=150)
    ax.plot(t_ms, ch[0], color=BLUE, lw=1.1, label="CH1  PA1/A1  (50 Hz test tone)")
    ax.plot(t_ms, ch[1], color=ORANGE, lw=1.1, label="CH2  PA4/A2  (120 Hz + 7 Hz)")
    c1, c2 = stats.channels
    ax.text(0.01, 0.03,
            f"last block:  CH1 mean {counts_to_mv(c1.mean):.0f} mV, AC rms "
            f"{counts_to_mv(c1.ac_rms):.0f} mV   |   CH2 mean {counts_to_mv(c2.mean):.0f} mV, "
            f"AC rms {counts_to_mv(c2.ac_rms):.0f} mV",
            transform=ax.transAxes, fontsize=9, family="monospace",
            bbox={"facecolor": "white", "alpha": 0.85, "edgecolor": "#cbd5e0"})
    ax.set(xlabel="time (ms)", ylabel="voltage (mV)", ylim=(0, 3300), xlim=(0, t_ms[-1]))
    ax.set_title("adc_viewer.py with --simulate (synthetic input, same frame format as firmware)",
                 fontsize=11)
    ax.legend(loc="upper right", fontsize=9)
    ax.grid(alpha=0.25)
    fig.tight_layout()
    fig.savefig(HERE / "viewer_simulated.png")
    plt.close(fig)


if __name__ == "__main__":
    timeline()
    front_end()
    viewer_snapshot()
    print("figures written to", HERE)
