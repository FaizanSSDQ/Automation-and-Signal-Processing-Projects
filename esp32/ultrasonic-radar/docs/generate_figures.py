"""
Regenerate the README figures:  python docs/generate_figures.py
Requires: matplotlib. The radar-page screenshot additionally needs Microsoft
Edge or Google Chrome (skipped if neither is found).

temperature_error.png  range error from a fixed speed of sound, against air temperature
sharp_curve.png        the Sharp GP2Y0A21 voltage-to-distance curve the firmware uses
radar_page.png         the real radar page from radar/web_ui.h, opened in demo mode

The formulas here are the same ones as radar/src/core/radar_core.c.

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
BLUE, ORANGE, GREEN, GREY = "#2b6cb0", "#dd6b20", "#2f855a", "#4a5568"


def clean(ax):
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
    ax.grid(True, alpha=0.3)


def sound_cm_per_us(temp_c):
    return (331.3 + 0.606 * temp_c) / 10000.0


def temperature_figure():
    temp = np.linspace(-10, 45, 111)
    fig, ax = plt.subplots(figsize=(7.5, 3.8))
    for dist, col in ((100, BLUE), (200, ORANGE)):
        echo_us = 2 * dist / sound_cm_per_us(temp)            # what the sensor really measures
        fixed = echo_us * 0.034 / 2                           # the old sketch: 0.034 cm/us always
        ax.plot(temp, fixed - dist, color=col, lw=2, label=f"fixed 0.034 cm/µs, target at {dist} cm")
    ax.axhline(0, color=GREEN, lw=2, label="temperature-corrected (this firmware)")
    ax.set_xlabel("air temperature (°C)")
    ax.set_ylabel("range error (cm)")
    ax.set_title("Why the speed of sound needs the air temperature")
    ax.legend(frameon=False, fontsize=9, loc="lower left")
    clean(ax)
    fig.tight_layout()
    fig.savefig(HERE / "temperature_error.png", dpi=150)
    plt.close(fig)
    print("wrote temperature_error.png")


def sharp_figure():
    volts = np.linspace(0.3, 3.1, 300)
    cm = 27.73 * volts ** -1.205
    ok = (cm >= 10) & (cm <= 80)
    fig, ax = plt.subplots(figsize=(7.5, 3.8))
    ax.plot(volts, cm, color=GREY, lw=1.5, ls="--", label="fitted curve outside the rated range")
    ax.plot(volts[ok], cm[ok], color=ORANGE, lw=2.5, label="used: 10 to 80 cm")
    ax.axhspan(10, 80, color=ORANGE, alpha=0.07)
    ax.set_xlabel("Sharp output (V)")
    ax.set_ylabel("distance (cm)")
    ax.set_ylim(0, 120)
    ax.set_title("Sharp GP2Y0A21: d = 27.73 · V^-1.205")
    ax.legend(frameon=False, fontsize=9)
    clean(ax)
    fig.tight_layout()
    fig.savefig(HERE / "sharp_curve.png", dpi=150)
    plt.close(fig)
    print("wrote sharp_curve.png")


def radar_page_screenshot():
    browsers = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        shutil.which("chromium") or "", shutil.which("google-chrome") or "", shutil.which("msedge") or "",
    ]
    browser = next((b for b in browsers if b and Path(b).exists()), None)
    if browser is None:
        print("no Edge/Chrome found: radar_page.png not updated")
        return
    src = (ROOT / "radar" / "web_ui.h").read_text(encoding="utf-8")
    html = src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
    with tempfile.TemporaryDirectory() as tmp:
        page = Path(tmp) / "index.html"
        page.write_text(html, encoding="utf-8")
        out = HERE / "radar_page.png"
        subprocess.run([browser, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--window-size=780,860",
                        "--virtual-time-budget=1500", f"--screenshot={out}", page.as_uri() + "?demo"],
                       check=True, capture_output=True)
    print("wrote", out.name)


if __name__ == "__main__":
    temperature_figure()
    sharp_figure()
    radar_page_screenshot()
    print("figures written to", HERE)
