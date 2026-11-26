"""Analysis of the DDS waveform generator output.

The samples come from build/dump_wave (``make dump``), which runs the
firmware's own command parser and DDS core, so every number here describes
the real firmware output. On top of that this module models the hardware:
the PWM stage (zero-order hold plus a carrier at 125 kHz) and the two-stage
RC reconstruction filter.

Examples:
    python tools/dds_analysis.py "wave sine" "freq 1k"
    python tools/dds_analysis.py "wave saw" "freq 4301" "aa off"

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from __future__ import annotations

import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

FS = 125_000.0          # sample rate = PWM frequency
FULL_SCALE = 512        # PWM compare value for 100 % duty
VREF = 3.3              # PWM high level (V)
BUILD = Path(__file__).resolve().parent.parent / "build"

# Reconstruction filter: PA6 - R1 - (C1 to GND) - R2 - (C2 to GND) - output
R1, C1, R2, C2 = 1e3, 10e-9, 10e3, 1e-9


def find_dump() -> Path:
    for name in ("dump_wave", "dump_wave.exe"):
        if (BUILD / name).is_file():
            return BUILD / name
    raise FileNotFoundError(f"dump_wave not found in {BUILD}; run 'make dump' first")


def dump(commands: list[str], n: int) -> tuple[np.ndarray, np.ndarray]:
    """Run the firmware DDS for n samples; returns (waveform, sync) PWM compare values."""
    out = subprocess.run([str(find_dump()), str(n), *commands], capture_output=True, check=True).stdout
    pairs = np.frombuffer(out, dtype="<u2").reshape(-1, 2)
    return pairs[:, 0].astype(float), pairs[:, 1].astype(float)


def volts(codes: np.ndarray) -> np.ndarray:
    return codes * VREF / FULL_SCALE


def tuning_word(freq_hz: float) -> int:
    """Same rounding as dds_tuning_word() in dds.c (input in milli-hertz there)."""
    mhz = round(freq_hz * 1000)
    return ((mhz << 32) + 62_500_000) // 125_000_000


# ---- reconstruction filter ------------------------------------------------------------------

def rc_response(f, r1=R1, c1=C1, r2=R2, c2=C2):
    """Complex response of the loaded two-stage RC low-pass."""
    s = 2j * np.pi * np.asarray(f, dtype=float)
    return 1.0 / (s * s * r1 * c1 * r2 * c2 + s * (r1 * c1 + r2 * c2 + r1 * c2) + 1.0)


def zoh_response(f):
    """Each sample is held for one PWM period: a sinc roll-off (no phase)."""
    return np.abs(np.sinc(np.asarray(f, dtype=float) / FS))


def corner_frequency() -> float:
    f = np.logspace(2, 6, 20000)
    return float(f[np.argmin(np.abs(20 * np.log10(np.abs(rc_response(f))) + 3.0))])


def ripple_vpp(duty: float = 0.5, harmonics: int = 20) -> float:
    """Peak-to-peak PWM ripple left after the filter, for a constant duty cycle."""
    t = np.linspace(0, 1 / FS, 2000, endpoint=False)
    v = np.zeros_like(t)
    for n in range(1, harmonics + 1):
        h = rc_response(n * FS)
        a = 2 * VREF / (n * np.pi) * np.sin(n * np.pi * duty)  # PWM Fourier series
        v += np.abs(h) * a * np.cos(2 * np.pi * n * FS * t - n * np.pi * duty + np.angle(h))
    return float(v.max() - v.min())


def filter_samples(x: np.ndarray, oversample: int = 8, rate: float = FS) -> tuple[np.ndarray, np.ndarray]:
    """Time response of the filter to piecewise-constant input values arriving at `rate`
    (default: one held value per PWM period). Exact discretisation of the 2-state circuit.
    Returns (t, v_out) at `oversample` points per input value."""
    # States: capacitor voltages v1, v2.  C1 v1' = (u - v1)/R1 - (v1 - v2)/R2;  C2 v2' = (v1 - v2)/R2
    a = np.array([[-(1 / R1 + 1 / R2) / C1, 1 / (R2 * C1)], [1 / (R2 * C2), -1 / (R2 * C2)]])
    b = np.array([1 / (R1 * C1), 0.0])
    dt = 1 / (rate * oversample)
    lam, vec = np.linalg.eig(a)
    ad = (vec @ np.diag(np.exp(lam * dt)) @ np.linalg.inv(vec)).real
    bd = np.linalg.solve(a, (ad - np.eye(2)) @ b)
    u = np.repeat(x, oversample)
    state = np.array([u[0], u[0]])
    out = np.empty_like(u)
    for k, uk in enumerate(u):
        state = ad @ state + bd * uk
        out[k] = state[1]
    return np.arange(len(u)) * dt, out


# ---- spectra --------------------------------------------------------------------------------

def spectrum_db(x: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Blackman-Harris windowed amplitude spectrum in dB relative to the largest component."""
    n = len(x)
    k = np.arange(n)
    w = (0.35875 - 0.48829 * np.cos(2 * np.pi * k / n) + 0.14128 * np.cos(4 * np.pi * k / n)
         - 0.01168 * np.cos(6 * np.pi * k / n))
    mag = np.abs(np.fft.rfft((x - x.mean()) * w))
    f = np.fft.rfftfreq(n, 1 / FS)
    db = 20 * np.log10(np.maximum(mag, 1e-12) / mag.max())
    return f, db


def _power_bins(x: np.ndarray):
    n = len(x)
    k = np.arange(n)
    w = (0.35875 - 0.48829 * np.cos(2 * np.pi * k / n) + 0.14128 * np.cos(4 * np.pi * k / n)
         - 0.01168 * np.cos(6 * np.pi * k / n))
    p = np.abs(np.fft.rfft((x - x.mean()) * w)) ** 2
    return np.fft.rfftfreq(n, 1 / FS), p


@dataclass
class Quality:
    thd_db: float        # harmonics 2..10 relative to the fundamental
    sfdr_db: float       # fundamental to the largest other component
    alias_db: float      # everything that is not a true harmonic, relative to the fundamental


def quality(x: np.ndarray, f0: float, span: int = 4) -> Quality:
    f, p = _power_bins(x)
    df = f[1]
    p[: span + 1] = 0.0  # ignore DC

    def band(fc):
        i = round(fc / df)
        return slice(max(i - span, 0), i + span + 1)

    fund = p[band(f0)].sum()
    harm_mask = np.zeros_like(p, dtype=bool)
    harm_power = 0.0
    n = 1
    while n * f0 < FS / 2 - span * df:
        sl = band(n * f0)
        harm_mask[sl] = True
        if 2 <= n <= 10:
            harm_power += p[sl].sum()
        n += 1
    other = p.copy()
    other[band(f0)] = 0.0
    alias = p[~harm_mask].sum()
    return Quality(thd_db=10 * np.log10(max(harm_power, 1e-30) / fund),
                   sfdr_db=10 * np.log10(fund / max(other.max(), 1e-30)),
                   alias_db=10 * np.log10(max(alias, 1e-30) / fund))


def measured_frequency(x: np.ndarray) -> float:
    """Frequency from interpolated rising mid-level crossings."""
    mid = (x.max() + x.min()) / 2
    y = x - mid
    idx = np.nonzero((y[:-1] < 0) & (y[1:] >= 0))[0]
    t = idx + (-y[idx] / (y[idx + 1] - y[idx]))
    return (len(t) - 1) / ((t[-1] - t[0]) / FS)


def main(argv: list[str]) -> int:
    commands = argv or ["wave sine", "freq 1k"]
    wave, _ = dump(commands, 1 << 17)
    f0_text = next((c.split()[1] for c in commands if c.startswith("freq")), "1k")
    f0 = float(f0_text.lower().removesuffix("hz").replace("k", "e3"))
    q = quality(volts(wave), f0)
    print(f"{' | '.join(commands)}")
    print(f"  measured frequency {measured_frequency(volts(wave)):.4f} Hz")
    print(f"  THD {q.thd_db:.1f} dB, SFDR {q.sfdr_db:.1f} dB, non-harmonic (alias + noise) {q.alias_db:.1f} dB")
    print(f"  filter: -3 dB at {corner_frequency():.0f} Hz, ripple {ripple_vpp() * 1e3:.0f} mVpp at 50 % duty")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
