"""Tests for the Python reference model (run: python -m pytest tools)."""

import numpy as np

import fsk_model as m


def test_tuning_words_match_firmware():
    assert m.tuning_word(m.MARK_HZ) == 2**28  # fsk_mod.c with fs = 16 kHz
    assert m.tuning_word(m.SPACE_HZ) == 2**29


def test_frame_bits_lsb_first():
    assert m.frame_bits(b"\x01") == [0, 1, 0, 0, 0, 0, 0, 0, 0, 1]
    assert len(m.frame_bits(b"AB")) == 20


def test_clean_round_trip():
    msg = b"Hello, FSK!"
    text, stats = m.demodulate(m.modulate(m.frame_bits(msg)))
    assert text == msg
    assert stats["framing_errors"] == 0


def test_noisy_round_trip():
    rng = np.random.default_rng(3)
    msg = b"noise test 123"
    rx = m.add_noise(m.modulate(m.frame_bits(msg)), m.ebn0_to_noise_rms(14), rng)
    assert m.demodulate(rx)[0] == msg


def test_ber_follows_theory():
    simulated = m.bit_error_rate(8.0, 40_000)
    theory = float(m.theoretical_ber(8.0))
    assert 0.5 * theory < simulated < 2.0 * theory
    assert m.bit_error_rate(16.0, 20_000) == 0.0
