"""
Regenerate the README figures:  make dump && python docs/generate_figures.py
Requires: numpy, matplotlib, schemdraw.

Waveform samples come from build/dump_wave, i.e. the firmware's own command
parser and DDS core. The analog side (PWM averaging, RC filter) is modeled.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import schemdraw
import schemdraw.elements as elm

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import dds_analysis as da

BLUE, ORANGE, GREEN, GREY, RED, PURPLE = "#2b6cb0", "#dd6b20", "#2f855a", "#4a5568", "#c53030", "#6b46c1"


def clean(ax):
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
    ax.grid(True, alpha=0.25)


def waveforms():
    """Each waveform at 1 kHz: PWM-averaged samples, filtered output and sync."""
    shapes = [("sine", []), ("square", ["duty 30"]), ("triangle", []), ("saw", []), ("noise", [])]
    fig, axes = plt.subplots(len(shapes), 1, figsize=(10, 9.5), dpi=150, sharex=True)
    n = 300  # 2.4 ms
    for ax, (name, extra) in zip(axes, shapes, strict=True):
        wave, sync = da.dump([f"wave {name}", "freq 1k", *extra], n)
        v = da.volts(wave)
        t_ms = np.arange(n) / da.FS * 1e3
        tf, vf = da.filter_samples(v)
        ax.step(t_ms, v, where="post", color=GREY, lw=0.8, alpha=0.7, label="PWM average per sample")
        ax.plot(tf * 1e3, vf, color=BLUE, lw=1.8, label="after RC filter")
        ax.step(t_ms, da.volts(sync) * 0.15 - 0.75, where="post", color=ORANGE, lw=1.0, label="sync (PA7, scaled)")
        ax.set_ylim(-0.9, 3.4)
        ax.set_ylabel("V")
        ax.text(0.005, 0.92, f"{name}" + (" 30 %" if extra else ""), transform=ax.transAxes,
                fontsize=10, fontweight="bold", va="top")
        clean(ax)
    axes[0].legend(loc="upper right", fontsize=8, ncol=3)
    axes[-1].set_xlabel("time (ms)")
    fig.suptitle("Firmware output at 1 kHz, 2 Vpp around 1.65 V (125 kS/s, 9-bit)", fontsize=11)
    fig.tight_layout()
    fig.savefig(HERE / "waveforms.png", facecolor="white")
    plt.close(fig)


def antialias_spectrum():
    """Saw at 4301 Hz: naive versus PolyBLEP, digital spectrum up to fs/2."""
    fig, axes = plt.subplots(2, 1, figsize=(10, 6.2), dpi=150, sharex=True)
    for ax, aa, color in ((axes[0], "off", RED), (axes[1], "on", GREEN)):
        wave, _ = da.dump(["wave saw", "freq 4301", f"aa {aa}"], 1 << 16)
        v = da.volts(wave)
        f, db = da.spectrum_db(v)
        q = da.quality(v, 4301.0)
        ax.plot(f / 1e3, db, color=color, lw=0.7)
        harmonics = np.arange(1, 15) * 4.301
        ax.plot(harmonics, np.full_like(harmonics, 3), "v", color=GREY, ms=4)
        ax.set_ylim(-110, 8)
        ax.set_ylabel("dB")
        title = "naive saw (anti-aliasing off)" if aa == "off" else "PolyBLEP saw (anti-aliasing on)"
        ax.set_title(f"{title}: non-harmonic energy {q.alias_db:.1f} dB", fontsize=10)
        clean(ax)
    axes[1].set_xlabel("frequency (kHz), up to fs/2 = 62.5 kHz")
    fig.suptitle("Aliasing of a 4301 Hz sawtooth (▼ = true harmonics; every other peak is an alias)", fontsize=11)
    fig.tight_layout()
    fig.savefig(HERE / "antialias_spectrum.png", facecolor="white")
    plt.close(fig)


def sine_spectrum():
    wave, _ = da.dump(["wave sine", "freq 1k", "amp 3.3"], 1 << 17)
    v = da.volts(wave)
    f, db = da.spectrum_db(v)
    q = da.quality(v, 1000.0)
    fig, ax = plt.subplots(figsize=(10, 4), dpi=150)
    ax.plot(f / 1e3, db, color=BLUE, lw=0.7, label="DDS samples (9-bit)")
    fo = np.logspace(1, np.log10(62_500), 400)
    ax.plot(fo / 1e3, 20 * np.log10(np.abs(da.rc_response(fo)) * da.zoh_response(fo)), color=ORANGE,
            lw=1.5, label="RC filter × sample-hold response")
    ax.set_ylim(-120, 8)
    ax.set_xlim(0, 62.5)
    ax.set_xlabel("frequency (kHz)")
    ax.set_ylabel("dB")
    ax.set_title(f"1 kHz sine, 3.3 Vpp: THD {q.thd_db:.1f} dB, SFDR {q.sfdr_db:.1f} dB", fontsize=10)
    ax.legend(fontsize=8.5)
    clean(ax)
    fig.tight_layout()
    fig.savefig(HERE / "sine_spectrum.png", facecolor="white")
    plt.close(fig)


def filter_response():
    """RC filter magnitude and the PWM ripple it leaves at the output."""
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 4), dpi=150, gridspec_kw={"width_ratios": [1.4, 1]})
    f = np.logspace(1, 6, 600)
    a1.semilogx(f, 20 * np.log10(np.abs(da.rc_response(f))), color=BLUE, lw=2, label="RC filter")
    fc = da.corner_frequency()
    a1.axvline(fc, color=GREY, ls="--", lw=1)
    a1.text(fc, -55, f" −3 dB\n {fc / 1e3:.1f} kHz", fontsize=8.5, color=GREY)
    att = 20 * np.log10(abs(da.rc_response(da.FS)))
    a1.plot(da.FS, att, "o", color=RED)
    a1.text(da.FS, att + 6, f"PWM carrier\n125 kHz: {att:.0f} dB", fontsize=8.5, color=RED, ha="center")
    a1.set_ylim(-70, 5)
    a1.set_xlabel("frequency (Hz)")
    a1.set_ylabel("dB")
    a1.set_title("Two-stage RC: 1 kΩ/10 nF + 10 kΩ/1 nF", fontsize=10)
    clean(a1)

    # Tick-accurate PWM at 50 % duty through the same filter, after settling.
    ticks = 64
    period = np.r_[np.full(ticks // 2, 3.3), np.zeros(ticks // 2)]
    u = np.tile(period, 400)
    _, v = da.filter_samples(u, oversample=1, rate=da.FS * ticks)
    keep = slice(len(u) - 5 * ticks, len(u))
    tt = (np.arange(len(u))[keep] - (len(u) - 5 * ticks)) / (da.FS * ticks) * 1e6
    a2.plot(tt, v[keep] * 1e3, color=BLUE, lw=1.5)
    a2.set_xlabel("time (µs), 5 PWM periods")
    a2.set_ylabel("output (mV)")
    ripple = (v[keep].max() - v[keep].min()) * 1e3
    a2.set_title(f"Residual ripple at 50 % duty: {ripple:.0f} mVpp", fontsize=10)
    clean(a2)
    fig.tight_layout()
    fig.savefig(HERE / "filter_response.png", facecolor="white")
    plt.close(fig)


def sweep_spectrogram():
    wave, _ = da.dump(["sweep 100 10k 2 log"], 250_000)
    v = da.volts(wave)
    fig, ax = plt.subplots(figsize=(10, 3.8), dpi=150)
    spec, _, _, im = ax.specgram(v - v.mean(), NFFT=4096, Fs=da.FS, noverlap=3584, cmap="viridis",
                                 window=np.blackman(4096))
    top = 10 * np.log10(spec.max())
    im.set_clim(top - 70, top)  # show the top 70 dB
    ax.set_yscale("log")
    ax.set_ylim(50, 40_000)
    ax.set_xlabel("time (s)")
    ax.set_ylabel("frequency (Hz)")
    ax.set_title("sweep 100 10k 2 log: logarithmic sweep 100 Hz → 10 kHz in 2 s (straight line on a log axis)",
                 fontsize=10)
    fig.tight_layout()
    fig.savefig(HERE / "sweep_spectrogram.png", facecolor="white")
    plt.close(fig)


def rc_schematic():
    with schemdraw.Drawing(file=str(HERE / "rc_filter.svg"), show=False) as d:
        d.config(fontsize=12, bgcolor="white", margin=0.6)
        d += elm.Dot(open=True).label("PA6 (D12)\nPWM 125 kHz", loc="left")
        d += elm.Resistor().right().label("R1\n1 kΩ")
        d += elm.Dot()
        d.push()
        d += elm.Capacitor().down().label("C1\n10 nF", loc="bottom")
        d += elm.Ground()
        d.pop()
        d += elm.Resistor().right().label("R2\n10 kΩ")
        d += elm.Dot()
        d.push()
        d += elm.Capacitor().down().label("C2\n1 nF", loc="bottom")
        d += elm.Ground()
        d.pop()
        d += elm.Line().right(1.5)
        d += elm.Dot(open=True).label("OUT\n0 – 3.3 V\n(load ≥ 100 kΩ\nor buffer)", loc="right")


if __name__ == "__main__":
    waveforms()
    antialias_spectrum()
    sine_spectrum()
    filter_response()
    sweep_spectrogram()
    rc_schematic()
    svg = HERE / "rc_filter.svg"
    svg.write_bytes(svg.read_bytes().replace(b"\r\n", b"\n"))  # keep LF on Windows too
    print("figures written to", HERE)
