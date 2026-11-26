"""Tests of the generator output, measured on samples from the firmware's own
DDS core (build/dump_wave) and on the reconstruction-filter model.

Build the dump tool first:  make dump        Run:  python -m pytest -q tools

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import dds_analysis as da
import numpy as np
import pytest

try:
    da.find_dump()
    HAVE_DUMP = True
except FileNotFoundError:
    HAVE_DUMP = False

needs_dump = pytest.mark.skipif(not HAVE_DUMP, reason="dump_wave not built (run 'make dump')")
N = 1 << 17  # ~1.05 s of output


# ---- models (no firmware needed) -------------------------------------------------------------

def test_tuning_word_matches_firmware_vectors():
    assert da.tuning_word(1000) == 34_359_738        # host C test: dds_tuning_word(1000000)
    assert da.tuning_word(0.01) == 344


def test_filter_design():
    assert abs(da.rc_response(0)) == pytest.approx(1.0)
    assert 9_000 < da.corner_frequency() < 10_000
    assert 20 * np.log10(abs(da.rc_response(5_000))) > -1.0     # flat to 1 dB up to 5 kHz
    assert 20 * np.log10(abs(da.rc_response(125_000))) < -35.0  # carrier suppressed
    assert 0.05 < da.ripple_vpp(0.5) < 0.08                      # ~2 % of full scale
    assert da.ripple_vpp(0.1) < da.ripple_vpp(0.5)               # worst case is 50 % duty


def test_filter_time_response_settles_to_input():
    _t, v = da.filter_samples(np.full(400, 2.0))
    assert v[-1] == pytest.approx(2.0, abs=1e-6)
    _, step = da.filter_samples(np.r_[np.zeros(50), np.ones(400)])
    assert 0.0 <= step.min() and step.max() <= 1.0 + 1e-9        # RC ladder: no overshoot


# ---- firmware output -------------------------------------------------------------------------

@needs_dump
def test_sine_purity_and_frequency():
    wave, _ = da.dump(["wave sine", "freq 1k", "amp 3.3"], N)
    v = da.volts(wave)
    q = da.quality(v, 1000.0)
    assert q.thd_db < -60.0
    assert q.sfdr_db > 65.0
    assert da.measured_frequency(v) == pytest.approx(1000.0, abs=1e-3)
    assert v.min() == pytest.approx(0.0, abs=0.01) and v.max() == pytest.approx(3.3, abs=0.01)


@needs_dump
def test_odd_frequency_is_exact():
    wave, _ = da.dump(["freq 1234.567"], N)
    assert da.measured_frequency(da.volts(wave)) == pytest.approx(1234.567, abs=2e-3)


@pytest.mark.parametrize("wave_name", ["saw", "square"])
@needs_dump
def test_polyblep_reduces_aliasing(wave_name):
    # 4301 Hz is not a divisor of 125 kHz, so aliases land between the harmonics.
    naive, _ = da.dump([f"wave {wave_name}", "freq 4301", "aa off"], N)
    blep, _ = da.dump([f"wave {wave_name}", "freq 4301", "aa on"], N)
    q_naive = da.quality(da.volts(naive), 4301.0)
    q_blep = da.quality(da.volts(blep), 4301.0)
    assert q_blep.alias_db < q_naive.alias_db - 12.0


@needs_dump
def test_square_duty_and_sync():
    wave, sync = da.dump(["wave square", "freq 1234.567", "duty 25", "aa off"], N)
    assert np.mean(wave > 256) == pytest.approx(0.25, abs=0.003)
    assert np.mean(sync > 0) == pytest.approx(0.5, abs=0.003)


@needs_dump
def test_log_sweep_rises_geometrically():
    _wave, sync = da.dump(["sweep 100 10k 1 log"], 125_000)
    edges = np.nonzero((sync[1:] > 0) & (sync[:-1] == 0))[0]
    freq = da.FS / np.diff(edges)
    t = edges[1:] / da.FS
    # Expected instantaneous frequency: 100 * 100^t
    expected = 100 * 100 ** t
    assert np.all(np.abs(freq / expected - 1) < 0.08)
    assert freq[-1] > 8_000


@needs_dump
def test_output_off_is_zero():
    wave, sync = da.dump(["off"], 2048)
    assert not wave.any() and not sync.any()
