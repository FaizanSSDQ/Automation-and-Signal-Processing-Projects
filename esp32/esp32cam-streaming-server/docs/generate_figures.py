"""
Regenerate the README screenshot:  python docs/generate_figures.py
Needs Microsoft Edge or Google Chrome (skipped if neither is found).

camera_page.png  the real page from camera_server/web_ui.h, opened in demo mode
                 (a moving test pattern stands in for the camera)

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import shutil
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent


def camera_page_screenshot():
    browsers = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        shutil.which("chromium") or "", shutil.which("google-chrome") or "", shutil.which("msedge") or "",
    ]
    browser = next((b for b in browsers if b and Path(b).exists()), None)
    if browser is None:
        print("no Edge/Chrome found: camera_page.png not updated")
        return
    src = (ROOT / "camera_server" / "web_ui.h").read_text(encoding="utf-8")
    html = src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
    with tempfile.TemporaryDirectory() as tmp:
        page = Path(tmp) / "index.html"
        page.write_text(html, encoding="utf-8")
        out = HERE / "camera_page.png"
        subprocess.run([browser, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--window-size=1000,640",
                        "--virtual-time-budget=2500", f"--screenshot={out}", page.as_uri() + "?demo"],
                       check=True, capture_output=True)
    print("wrote", out.name)


if __name__ == "__main__":
    camera_page_screenshot()
