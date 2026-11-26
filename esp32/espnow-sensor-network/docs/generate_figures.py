"""
Regenerate the README figures:  python docs/generate_figures.py
Requires: matplotlib, numpy. The dashboard screenshot also needs Microsoft Edge
or Google Chrome (skipped if neither is found).

battery_life.png  estimate of node battery life against the report interval
dashboard.png     the real dashboard from sensor_network/dashboard.h in demo mode

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

# Assumptions (estimates, to be replaced by measurements with a power profiler):
BATTERY_MAH = 2000          # 18650 Li-ion cell
AWAKE_S = 0.35              # boot, DHT22 + BMP280, ESP-NOW init and one send
AWAKE_MA = 100.0            # average current while awake, radio on
SLEEP_UA = {                # deep-sleep current of the whole node
    "bare module + LDO (ESP32 ~10 µA, DHT22 ~45 µA, divider 20 µA)": 80.0,
    "typical DevKit board (USB chip, AMS1117 regulator, LED)": 10000.0,
}


def battery_life():
    interval = np.logspace(np.log10(10), np.log10(3600), 200)
    fig, ax = plt.subplots(figsize=(9, 4.2), dpi=150)
    for (label, sleep_ua), color in zip(SLEEP_UA.items(), ("#2b6cb0", "#dd6b20"), strict=True):
        avg_ma = (AWAKE_MA * AWAKE_S + sleep_ua / 1000.0 * (interval - AWAKE_S)) / interval
        days = BATTERY_MAH / avg_ma / 24.0
        ax.loglog(interval, days, color=color, lw=2.2, label=label)
        d60 = BATTERY_MAH / ((AWAKE_MA * AWAKE_S + sleep_ua / 1000.0 * (60 - AWAKE_S)) / 60) / 24
        ax.plot(60, d60, "o", color=color)
        ax.annotate(f"{d60:.0f} days", (60, d60), xytext=(8, 6), textcoords="offset points", fontsize=9,
                    color=color)
    ax.axvline(60, color="#a0aec0", ls=":", lw=1)
    ax.set_xlabel("report interval (s)")
    ax.set_ylabel("battery life (days)")
    ax.set_title(f"Estimated life on a {BATTERY_MAH} mAh cell ({AWAKE_S * 1000:.0f} ms awake at "
                 f"{AWAKE_MA:.0f} mA per report)", fontsize=10)
    ax.grid(True, which="both", alpha=0.25)
    ax.legend(fontsize=8.5, loc="upper left")
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
    fig.tight_layout()
    fig.savefig(HERE / "battery_life.png", facecolor="white")
    plt.close(fig)


def dashboard_screenshot():
    browsers = [r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
                r"C:\Program Files\Google\Chrome\Application\chrome.exe",
                shutil.which("chromium") or "", shutil.which("google-chrome") or ""]
    browser = next((b for b in browsers if b and Path(b).exists()), None)
    if browser is None:
        print("no Edge/Chrome found: dashboard.png not updated")
        return
    src = (ROOT / "sensor_network" / "dashboard.h").read_text(encoding="utf-8")
    html = src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
    with tempfile.TemporaryDirectory() as tmp:
        page = Path(tmp) / "index.html"
        page.write_text(html, encoding="utf-8")
        subprocess.run([browser, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--window-size=1100,520",
                        "--virtual-time-budget=3000", f"--screenshot={HERE / 'dashboard.png'}",
                        page.as_uri() + "?demo"], check=True, capture_output=True)


if __name__ == "__main__":
    battery_life()
    dashboard_screenshot()
    print("figures written to", HERE)
