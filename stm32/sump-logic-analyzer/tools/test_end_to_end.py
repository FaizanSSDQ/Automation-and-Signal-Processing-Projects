"""End-to-end tests: the Python client against the host simulator, which runs
the firmware's own SUMP parser, trigger and capture engine (tests/sim_device.c)
on modeled demo signals. Skipped if the simulator has not been built.

Build it first:  make sim        Run:  python -m pytest -q tools

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import itertools

import pytest
import sump_capture as sc

try:
    sc.find_simulator()
except FileNotFoundError:
    pytest.skip("host simulator not built (run 'make sim')", allow_module_level=True)


@pytest.fixture
def la():
    port = sc.SimPort()
    analyzer = sc.Analyzer(port)
    analyzer.reset()
    yield analyzer
    port.close()


def ch(data, n):
    return [(s >> n) & 1 for s in data]


def test_identity_and_metadata(la):
    assert la.identify() == "1ALS"
    meta = la.metadata()
    assert meta["device"] == "STM32F103 Logic Analyzer"
    assert (meta["probes"], meta["sample_memory"], meta["max_rate"]) == (8, 8192, 4_000_000)


def test_pwm_demo_signals(la):
    rate = 1e6
    data = la.capture(sc.divider_for(rate), 8192, 8192, sc.Trigger())
    assert len(data) == 8192
    # Duty cycles of the 10 kHz 50 %, 10 kHz 25 % and 1 kHz 10 % outputs.
    assert sum(ch(data, 0)) / 8192 == pytest.approx(0.50, abs=0.01)
    assert sum(ch(data, 1)) / 8192 == pytest.approx(0.25, abs=0.01)
    assert sum(ch(data, 2)) / 8192 == pytest.approx(0.10, abs=0.02)
    # 10 kHz at 1 MS/s: rising edges 100 samples apart.
    c0 = ch(data, 0)
    rises = [i for i in range(1, len(c0)) if c0[i] and not c0[i - 1]]
    assert all(b - a == 100 for a, b in itertools.pairwise(rises))
    # Unconnected inputs read low.
    assert all(s & 0xF0 == 0 for s in data)


def test_edge_trigger_lands_at_pretrigger_position(la):
    rate = 1e6
    samples, post = 2048, 1536
    data = la.capture(sc.divider_for(rate), samples, post, sc.parse_trigger("0:r"))
    t = samples - post
    c0 = ch(data, 0)
    assert c0[t - 1] == 0 and c0[t] == 1  # the trigger sample is the rising edge


def test_uart_trigger_and_decode(la):
    rate = sc.actual_rate(sc.divider_for(100e3))
    samples = 4096
    pre = 164  # 1.64 ms: less than the 3 ms before the first message starts
    data = la.capture(sc.divider_for(100e3), samples, samples - pre, sc.parse_trigger("3:f"))
    assert ch(data, 3)[pre] == 0 and ch(data, 3)[pre - 1] == 1  # start bit
    frames = sc.decode_uart(data, 3, rate, 9600)
    assert bytes(b for _, b in frames) == b"LA demo 0000\r\n"
    assert frames[0][0] == pre


def test_level_trigger_with_two_conditions(la):
    # CH1 high while CH2 high: the 25 % pulse overlapping the 1 kHz pulse.
    data = la.capture(sc.divider_for(1e6), 1024, 512, sc.parse_trigger("1:1,2:1"))
    assert data[512] & 0b110 == 0b110


def test_reset_after_capture_and_repeat(la):
    for _ in range(3):
        data = la.capture(sc.divider_for(500e3), 256, 256, sc.Trigger())
        assert len(data) == 256
    assert la.identify() == "1ALS"
