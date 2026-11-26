"""
Regenerate the README figures:  make sim && python docs/generate_figures.py
Requires: matplotlib. The control-page screenshot additionally needs Microsoft
Edge or Google Chrome (skipped if neither is found).

safety.png      the firmware's control core (build/sim_drive) on a 1-D car model
control_page.png the real control page from robot_car/web_ui.h, opened in demo mode

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import csv
import io
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BLUE, ORANGE, GREEN, GREY, RED = "#2b6cb0", "#dd6b20", "#2f855a", "#4a5568", "#c53030"


def run_sim(scenario: str) -> dict[str, list]:
    exe = next((p for p in (ROOT / "build" / "sim_drive", ROOT / "build" / "sim_drive.exe") if p.exists()), None)
    if exe is None:
        sys.exit("build/sim_drive not found: run 'make sim' first")
    text = subprocess.run([str(exe), scenario], capture_output=True, text=True, check=True).stdout
    rows = list(csv.DictReader(io.StringIO(text)))
    cols = {k: [r[k] for r in rows] for k in rows[0]}
    for k in ("t_s", "distance_cm", "joystick", "command", "wheel"):
        cols[k] = [float(v) for v in cols[k]]
    return cols


def clean(ax):
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
    ax.grid(True, alpha=0.3)


def safety_figure():
    wall, link = run_sim("wall"), run_sim("link")
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 3.8), dpi=150)

    a1.plot(wall["t_s"], wall["distance_cm"], color=BLUE, lw=2, label="distance to wall (cm)")
    a1.axhline(15, color=RED, ls="--", lw=1)
    a1.text(0.1, 18, "stop distance 15 cm", color=RED, ha="left", fontsize=8.5)
    a1.axhline(60, color=GREY, ls=":", lw=1)
    a1.text(4.9, 62, "slow-down starts at 60 cm", color=GREY, ha="right", fontsize=8.5)
    a1.set_xlabel("time (s)")
    a1.set_ylabel("cm")
    b = a1.twinx()
    b.plot(wall["t_s"], [w * 100 for w in wall["wheel"]], color=ORANGE, lw=1.6, label="wheel speed (%)")
    b.set_ylabel("wheel speed (%)", color=ORANGE)
    b.set_ylim(0, 110)
    a1.set_title("Joystick held at full forward towards a wall", fontsize=10)
    clean(a1)

    a2.plot(link["t_s"], [j * 100 for j in link["joystick"]], color=GREY, lw=1.5, ls="--", label="phone sends")
    a2.plot(link["t_s"], [w * 100 for w in link["wheel"]], color=ORANGE, lw=2, label="wheel speed")
    a2.axvline(2.0, color=RED, lw=1)
    a2.text(2.03, 85, "link lost", color=RED, fontsize=8.5)
    a2.axvspan(2.0, 2.5, color=RED, alpha=0.08)
    a2.text(2.25, 15, "500 ms\nwatchdog", ha="center", fontsize=8, color=RED)
    a2.set_xlim(1.0, 3.5)
    a2.set_ylim(-5, 100)
    a2.set_xlabel("time (s)")
    a2.set_ylabel("%")
    a2.legend(fontsize=8.5, loc="upper right")
    a2.set_title("Phone disconnects while driving at 70 %", fontsize=10)
    clean(a2)

    fig.suptitle("Safety rules of the control loop (firmware core, 50 Hz, simulated car)", fontsize=11)
    fig.tight_layout()
    fig.savefig(HERE / "safety.png", facecolor="white")
    plt.close(fig)


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
    src = (ROOT / "robot_car" / "web_ui.h").read_text(encoding="utf-8")
    html = src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
    with tempfile.TemporaryDirectory() as tmp:
        page = Path(tmp) / "index.html"
        page.write_text(html, encoding="utf-8")
        out = HERE / "control_page.png"
        subprocess.run([browser, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--window-size=500,650",
                        "--virtual-time-budget=3000", f"--screenshot={out}", page.as_uri() + "?demo"],
                       check=True, capture_output=True)
    print("wrote", out.name)


if __name__ == "__main__":
    safety_figure()
    control_page_screenshot()
    print("figures written to", HERE)
