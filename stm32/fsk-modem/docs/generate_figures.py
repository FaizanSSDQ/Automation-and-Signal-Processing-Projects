"""
Regenerate the README figures:  python docs/generate_figures.py
Requires: numpy, matplotlib, schemdraw.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
import schemdraw  # noqa: E402
import schemdraw.elements as elm  # noqa: E402

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import fsk_model as m  # noqa: E402

BLUE, ORANGE, GREEN, GREY, RED = "#2b6cb0", "#dd6b20", "#2f855a", "#4a5568", "#c53030"
VOLT = 3.3 / 4095


def character_frame() -> None:
    """Bits of the character 'A' and the FSK waveform that carries them."""
    char = ord("A")
    bits = m.frame_bits(bytes([char]))
    idle = 2
    x = m.modulate(bits, idle_before=idle, idle_after=2) * VOLT
    t = np.arange(len(x)) / m.FS * 1000
    seq = [1] * idle + bits + [1] * 2
    labels = ["idle"] * idle + ["start"] + [f"D{i}" for i in range(8)] + ["stop"] + ["idle"] * 2

    fig, (a1, a2) = plt.subplots(2, 1, figsize=(11, 4.8), dpi=150, sharex=True,
                                 gridspec_kw={"height_ratios": [1, 2]})
    tb = np.repeat(seq, 2)
    tt = np.repeat(np.arange(len(seq) + 1), 2)[1:-1]
    a1.plot(tt, tb, color=GREY, lw=1.6)
    for k, (b, lab) in enumerate(zip(seq, labels, strict=True)):
        color = GREY if lab == "idle" else (BLUE if b else ORANGE)
        a1.axvspan(k, k + 1, color=color, alpha=0.10 if lab == "idle" else 0.22)
        a1.text(k + 0.5, 1.25, lab, ha="center", fontsize=8)
        a1.text(k + 0.5, 0.45, str(b), ha="center", fontsize=9, weight="bold")
    a1.set(ylim=(-0.2, 1.55), yticks=[0, 1], ylabel="bit")
    a1.set_title(f"Character 'A' (0x{char:02X}) framed as start + 8 data bits (LSB first) + stop",
                 fontsize=11)
    a2.plot(t, x, color=GREY, lw=0.9)
    for k, b in enumerate(seq):
        a2.axvspan(k, k + 1, color=BLUE if b else ORANGE, alpha=0.12)
    a2.set(xlabel="time (ms)  —  1 bit = 1 ms", ylabel="DAC output (V)", xlim=(0, len(seq)))
    a2.text(0.15, 2.85, "mark = 1 kHz (1 cycle/bit)", color=BLUE, fontsize=9)
    a2.text(3.15, 2.85, "space = 2 kHz (2 cycles/bit)", color=ORANGE, fontsize=9)
    a2.text(len(seq) - 0.1, 2.85, "phase-continuous: no jumps at bit edges", ha="right",
            fontsize=8, color=GREY)
    for ax in (a1, a2):
        ax.grid(axis="x", alpha=0.25)
        ax.set_xticks(range(0, len(seq) + 1))
    fig.tight_layout()
    fig.savefig(HERE / "character_frame.png")
    plt.close(fig)


def spectrum() -> None:
    rng = np.random.default_rng(5)
    bits = m.frame_bits(rng.integers(32, 127, 400).astype(np.uint8).tobytes())
    x = (m.modulate(bits) - m.DAC_MID) / m.DAC_AMPLITUDE
    seg = 1024
    win = np.hanning(seg)
    frames = [x[i : i + seg] * win for i in range(0, len(x) - seg, seg // 2)]
    psd = np.mean([np.abs(np.fft.rfft(f)) ** 2 for f in frames], axis=0)
    f = np.fft.rfftfreq(seg, 1 / m.FS)
    fig, ax = plt.subplots(figsize=(11, 3.4), dpi=150)
    ax.plot(f, 10 * np.log10(psd / psd.max()), color=GREY, lw=1)
    for freq, lab, col in ((m.MARK_HZ, "mark 1 kHz", BLUE), (m.SPACE_HZ, "space 2 kHz", ORANGE)):
        ax.axvline(freq, color=col, ls="--", lw=1)
        ax.text(freq + 60, -6, lab, color=col, fontsize=9)
    ax.set(xlim=(0, m.FS / 2), ylim=(-60, 3), xlabel="frequency (Hz)",
           ylabel="power (dB, normalised)")
    ax.set_title("Spectrum of the transmitted signal (random text, 1000 baud)", fontsize=11)
    ax.grid(alpha=0.25)
    fig.tight_layout()
    fig.savefig(HERE / "spectrum.png")
    plt.close(fig)


def demod_internals() -> None:
    rng = np.random.default_rng(11)
    msg = b"Hi"
    rx = m.add_noise(m.modulate(m.frame_bits(msg), idle_before=2, idle_after=2),
                     m.ebn0_to_noise_rms(12), rng)
    text, stats = m.demodulate(rx)
    e_mark, e_space = m.correlator_energies(rx)
    scale = max(e_mark.max(), e_space.max())
    t = np.arange(len(rx)) / m.FS * 1000

    fig, (a1, a2, a3) = plt.subplots(3, 1, figsize=(11, 6.2), dpi=150, sharex=True,
                                     gridspec_kw={"height_ratios": [1.3, 1.3, 0.8]})
    a1.plot(t, rx * VOLT, color=GREY, lw=0.8)
    a1.set(ylabel="ADC input (V)")
    a1.set_title(f"Receiver internals with noise (Eb/N0 = 12 dB): decoded {text!r}", fontsize=11)
    a2.plot(t, e_mark / scale, color=BLUE, lw=1.2, label="mark energy (1 kHz correlator)")
    a2.plot(t, e_space / scale, color=ORANGE, lw=1.2, label="space energy (2 kHz correlator)")
    a2.set(ylabel="energy (norm.)")
    a2.legend(loc="upper right", fontsize=8)
    a3.step(t, stats["bits"], where="post", color=GREY, lw=1.2, label="bit decision")
    pts = np.array(stats["sample_points"])
    a3.plot(t[pts], stats["bits"][pts], "o", color=GREEN, ms=5, label="sampling instants")
    a3.set(ylabel="bit", yticks=[0, 1], ylim=(-0.3, 2.0), xlabel="time (ms)")
    a3.legend(loc="upper right", fontsize=8, ncol=2)
    for ax in (a1, a2, a3):
        ax.grid(alpha=0.25)
    fig.tight_layout()
    fig.savefig(HERE / "demod_internals.png")
    plt.close(fig)


def ber_curve() -> None:
    ebn0 = np.arange(0, 13, 1.0)
    sim = [m.bit_error_rate(e, 60_000, seed=int(e) + 1) for e in ebn0]
    fine = np.linspace(0, 13, 200)
    fig, ax = plt.subplots(figsize=(7.5, 4.6), dpi=150)
    ax.semilogy(fine, m.theoretical_ber(fine), color=GREY, lw=1.5,
                label=r"theory, non-coherent BFSK: $\frac{1}{2}e^{-E_b/2N_0}$")
    ax.semilogy(ebn0, np.maximum(sim, 1e-6), "o", color=BLUE, ms=6,
                label="simulated: this modem's correlator detector")
    ax.set(xlabel="Eb/N0 (dB)", ylabel="bit error rate", ylim=(1e-5, 1), xlim=(0, 13))
    ax.set_title("Bit error rate in white Gaussian noise", fontsize=11)
    ax.grid(which="both", alpha=0.25)
    ax.legend(fontsize=9, loc="lower left")
    fig.tight_layout()
    fig.savefig(HERE / "ber_curve.png")
    plt.close(fig)


def loopback_wiring() -> None:
    d = schemdraw.Drawing(show=False)
    d.config(fontsize=11, unit=2.8, bgcolor="white", margin=0.6)
    d.add(elm.Dot(open=True).label("PA4 / A2\nDAC_OUT1 (TX)", "left"))
    d.add(elm.Resistor().right().label("R1  1 kΩ"))
    node = d.add(elm.Dot())
    d.add(elm.Line().right().length(2.2))
    d.add(elm.Dot(open=True).label("PA1 / A1\nADC_IN1 (RX)", "right"))
    d.add(elm.Capacitor().down().at(node.center).label("C1\n47 nF", loc="top"))
    d.add(elm.Ground())
    d.add(elm.Label().at((node.center[0] - 0.3, node.center[1] - 4.2)).label(
        "RC reconstruction filter: fc = 1 / (2π · 1 kΩ · 47 nF) ≈ 3.4 kHz\n"
        "smooths the 16 kHz DAC steps, passes both tones (1 and 2 kHz).\n"
        "A plain jumper wire also works; two boards: TX → RX and GND → GND.",
        fontsize=10))
    d.save(str(HERE / "loopback_wiring.svg"))


if __name__ == "__main__":
    character_frame()
    spectrum()
    demod_internals()
    ber_curve()
    loopback_wiring()
    print("figures written to", HERE)
