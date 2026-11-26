"""
Regenerate the README figures:  make dump && python docs/generate_figures.py
Requires: numpy, matplotlib. The page screenshot additionally needs Microsoft
Edge or Google Chrome (skipped if neither is found).

waveforms.png   DAC codes from the firmware's DDS core (build/dump_wave) and the
                output after the 1 kOhm / 10 nF RC filter
antialias.png   spectrum of a 7.3 kHz square wave with and without PolyBLEP
control_page.png the real control page from wavegen/web_ui.h, opened in demo mode

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import shutil
import subprocess
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
FS = 100_000
VREF = 3.3
ORANGE, BLUE, GREY = "#dd6b20", "#2b6cb0", "#718096"


def dump(wave: str, freq: float, antialias: int, n: int) -> tuple[np.ndarray, np.ndarray]:
    exe = next(p for p in (ROOT / "build" / "dump_wave", ROOT / "build" / "dump_wave.exe") if p.exists())
    text = subprocess.run([str(exe), wave, str(freq), str(antialias), str(n)],
                          capture_output=True, text=True, check=True).stdout
    data = np.array([[int(v) for v in line.split()] for line in text.splitlines()])
    return data[:, 0] * VREF / 255, data[:, 1] * VREF / 255


def rc_filter(held: np.ndarray, tau: float, over: int = 20) -> tuple[np.ndarray, np.ndarray]:
    """Exact response of a first-order RC low-pass to the DAC's held samples."""
    dt = 1 / FS / over
    a = np.exp(-dt / tau)
    x = np.repeat(held, over)
    y = np.empty_like(x)
    acc = held[0]
    for i, v in enumerate(x):
        acc = v + (acc - v) * a
        y[i] = acc
    return np.arange(len(x)) * dt, y


def clean(ax):
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
    ax.grid(True, alpha=0.3)


def waveforms_figure():
    fig, axes = plt.subplots(2, 2, figsize=(10, 5.6), sharex=True)
    for ax, wave in zip(axes.flat, ("sine", "square", "triangle", "saw")):
        v, _ = dump(wave, 2500, 1, 80)                      # two cycles, 40 samples each
        t_ms = np.arange(len(v)) / FS * 1000
        ax.step(t_ms, v, where="post", color=GREY, lw=1, label="DAC (GPIO 25)")
        tf, vf = rc_filter(v, 1e3 * 10e-9)
        ax.plot(tf * 1000, vf, color=ORANGE, lw=2, label="after 1 kΩ / 10 nF")
        ax.set_title(f"{wave}, 2.5 kHz", fontsize=11)
        ax.set_ylim(0, 3.4)
        clean(ax)
    for ax in axes[1]:
        ax.set_xlabel("time (ms)")
    for ax in axes[:, 0]:
        ax.set_ylabel("volts")
    axes[0, 0].legend(frameon=False, fontsize=9, loc="lower left")
    fig.suptitle("Samples from the firmware's DDS core, 100 k samples/s, 8-bit", fontsize=12)
    fig.tight_layout()
    fig.savefig(HERE / "waveforms.png", dpi=150)
    plt.close(fig)
    print("wrote waveforms.png")


def antialias_figure():
    n = 16384
    fig, ax = plt.subplots(figsize=(10, 3.8))
    for aa, col, label in ((0, GREY, "naive square"), (1, ORANGE, "with PolyBLEP")):
        v, _ = dump("square", 7300, aa, n)
        spec = np.abs(np.fft.rfft((v - v.mean()) * np.hanning(n)))
        db = 20 * np.log10(spec / spec.max() + 1e-9)
        f = np.fft.rfftfreq(n, 1 / FS) / 1000
        ax.plot(f, db, color=col, lw=1.2 if aa else 1.0, label=label, alpha=1 if aa else 0.8)
    for k in (1, 3, 5):
        ax.axvline(7.3 * k, color=BLUE, lw=0.8, ls=":")
    ax.text(7.3 * 5 + 0.4, -8, "true harmonics\n7.3, 21.9, 36.5 kHz", color=BLUE, fontsize=9, va="top")
    ax.set_xlim(0, 50)
    ax.set_ylim(-90, 5)
    ax.set_xlabel("frequency (kHz)")
    ax.set_ylabel("level (dB)")
    ax.set_title("7.3 kHz square wave: everything that is not a dotted line is an alias")
    ax.legend(frameon=False, fontsize=9, loc="upper right")
    clean(ax)
    fig.tight_layout()
    fig.savefig(HERE / "antialias.png", dpi=150)
    plt.close(fig)
    print("wrote antialias.png")


def control_page_screenshot():
    browsers = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        shutil.which("chromium") or "", shutil.which("google-chrome") or "", shutil.which("msedge") or "",
    ]
    browser = next((b for b in browsers if b and Path(b).exists()), None)
    if browser is None:
        print("no Edge/Chrome found: control_page.png not updated")
        return
    src = (ROOT / "wavegen" / "web_ui.h").read_text(encoding="utf-8")
    html = src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
    with tempfile.TemporaryDirectory() as tmp:
        page = Path(tmp) / "index.html"
        page.write_text(html, encoding="utf-8")
        out = HERE / "control_page.png"
        subprocess.run([browser, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--window-size=1000,900",
                        "--virtual-time-budget=1500", f"--screenshot={out}", page.as_uri() + "?demo&wave=square"],
                       check=True, capture_output=True)
    print("wrote", out.name)


if __name__ == "__main__":
    waveforms_figure()
    antialias_figure()
    control_page_screenshot()
    print("figures written to", HERE)
