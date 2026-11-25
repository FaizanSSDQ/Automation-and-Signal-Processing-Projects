"""
Python reference model of the STM32 FSK modem (same parameters and algorithm as the C code).

Used to generate the README figures, to measure bit-error rate against theory, and as an
executable specification of fsk_mod.c / fsk_demod.c.

Author : Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
License: MIT
"""

from __future__ import annotations

import numpy as np

FS = 16_000  # sample rate (Hz)
BAUD = 1_000
SPB = FS // BAUD  # samples per bit
MARK_HZ = 1_000  # logic 1 / idle
SPACE_HZ = 2_000  # logic 0
DAC_MID = 2048
DAC_AMPLITUDE = 1240
SQUELCH_AMPLITUDE = 120


def tuning_word(freq_hz: int) -> int:
    """32-bit NCO increment, identical to fsk_mod.c."""
    return (freq_hz << 32) // FS


def frame_bits(text: bytes) -> list[int]:
    """UART-style framing: start (0), 8 data bits LSB first, stop (1)."""
    bits: list[int] = []
    for c in text:
        bits += [0] + [(c >> i) & 1 for i in range(8)] + [1]
    return bits


def modulate(bits: list[int], idle_before: int = 4, idle_after: int = 4) -> np.ndarray:
    """Phase-continuous FSK as 12-bit DAC codes (NCO with a 256-entry Q15 sine table)."""
    sine = np.round(32767 * np.sin(2 * np.pi * np.arange(256) / 256)).astype(np.int64)
    inc = {1: tuning_word(MARK_HZ), 0: tuning_word(SPACE_HZ)}
    seq = [1] * idle_before + list(bits) + [1] * idle_after
    out = np.empty(len(seq) * SPB, dtype=np.int64)
    phase = 0
    k = 0
    for b in seq:
        for _ in range(SPB):
            out[k] = DAC_MID + ((DAC_AMPLITUDE * sine[phase >> 24]) >> 15)
            phase = (phase + inc[b]) & 0xFFFF_FFFF
            k += 1
    return out


def correlator_energies(x: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Sliding one-bit correlator energies for mark and space (window ends at each sample)."""
    n = np.arange(len(x))
    centred = x.astype(np.float64) - DAC_MID
    kernel = np.ones(SPB)

    def energy(freq: float) -> np.ndarray:
        w = 2 * np.pi * freq / FS
        i = np.convolve(centred * np.cos(w * n), kernel)[: len(x)]
        q = np.convolve(centred * np.sin(w * n), kernel)[: len(x)]
        return (16384 * i) ** 2 + (16384 * q) ** 2  # same scale as the Q14 C code

    return energy(MARK_HZ), energy(SPACE_HZ)


def demodulate(x: np.ndarray) -> tuple[bytes, dict]:
    """Full receiver (detector + async state machine), mirroring fsk_demod.c."""
    e_mark, e_space = correlator_energies(x)
    squelch = (SQUELCH_AMPLITUDE * 16384 * SPB / 2) ** 2
    signal = (e_mark + e_space) > squelch
    bits = np.where(~signal | (e_mark >= e_space), 1, 0)

    out = bytearray()
    stats = {"framing_errors": 0, "false_starts": 0, "sample_points": []}
    state, countdown, nbit, shift, prev = "HUNT", 0, 0, 0, 1
    for k, b in enumerate(bits):
        if state == "HUNT":
            if prev == 1 and b == 0:
                state, countdown = "START", SPB // 2
        else:
            countdown -= 1
            if countdown == 0:
                stats["sample_points"].append(k)
                if state == "START":
                    if b == 0:
                        state, countdown, nbit, shift = "DATA", SPB, 0, 0
                    else:
                        stats["false_starts"] += 1
                        state = "HUNT"
                elif state == "DATA":
                    shift = (shift >> 1) | (int(b) << 7)
                    nbit += 1
                    countdown = SPB
                    if nbit == 8:
                        state = "STOP"
                elif state == "STOP":
                    if b == 1:
                        out.append(shift)
                    else:
                        stats["framing_errors"] += 1
                    state = "HUNT"
        prev = b
    stats["bits"] = bits
    return bytes(out), stats


def add_noise(x: np.ndarray, noise_rms: float, rng: np.random.Generator) -> np.ndarray:
    return np.clip(np.round(x + rng.normal(0.0, noise_rms, len(x))), 0, 4095)


def ebn0_to_noise_rms(ebn0_db: float, amplitude: float = DAC_AMPLITUDE) -> float:
    """Per-sample noise RMS for a given Eb/N0: Eb = A^2*SPB/2, N0 = 2*sigma^2."""
    ebn0 = 10 ** (ebn0_db / 10)
    return float(np.sqrt(amplitude**2 * SPB / (4 * ebn0)))


def bit_error_rate(ebn0_db: float, n_bits: int, seed: int = 1) -> float:
    """BER of the correlator detector with bit-aligned decisions (random data)."""
    rng = np.random.default_rng(seed)
    data = rng.integers(0, 2, n_bits).tolist()
    tx = modulate(data, idle_before=0, idle_after=0).astype(np.float64)
    rx = tx + rng.normal(0.0, ebn0_to_noise_rms(ebn0_db), len(tx))
    e_mark, e_space = correlator_energies(rx)
    decisions = np.where(e_mark[SPB - 1 :: SPB] >= e_space[SPB - 1 :: SPB], 1, 0)
    return float(np.mean(decisions != np.array(data)))


def theoretical_ber(ebn0_db: np.ndarray) -> np.ndarray:
    """Non-coherent orthogonal BFSK: Pb = 0.5 * exp(-Eb / (2 N0))."""
    return 0.5 * np.exp(-(10 ** (np.asarray(ebn0_db) / 10)) / 2)
