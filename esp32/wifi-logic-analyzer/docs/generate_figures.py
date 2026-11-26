"""
Regenerate the README screenshot:  python docs/generate_figures.py
Needs Microsoft Edge or Google Chrome (skipped if neither is found).

viewer.png       the real page from analyzer/web_ui.h, opened in demo mode
viewer_zoom.png  the same capture zoomed in on the decoded UART
            (synthetic UART, PWM, I2C and counter signals)

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import shutil
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent


def viewer_screenshot():
    browsers = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        shutil.which("chromium") or "", shutil.which("google-chrome") or "", shutil.which("msedge") or "",
    ]
    browser = next((b for b in browsers if b and Path(b).exists()), None)
    if browser is None:
        print("no Edge/Chrome found: viewer.png not updated")
        return
    src = (ROOT / "analyzer" / "web_ui.h").read_text(encoding="utf-8")
    html = src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
    with tempfile.TemporaryDirectory() as tmp:
        page = Path(tmp) / "index.html"
        page.write_text(html, encoding="utf-8")
        for name, query, height in (("viewer.png", "?demo", 820), ("viewer_zoom.png", "?demo&view=1590-2420", 520)):
            out = HERE / name
            subprocess.run([browser, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                            f"--window-size=1200,{height}", "--virtual-time-budget=2500", f"--screenshot={out}",
                            page.as_uri() + query], check=True, capture_output=True)
            print("wrote", out.name)


if __name__ == "__main__":
    viewer_screenshot()
