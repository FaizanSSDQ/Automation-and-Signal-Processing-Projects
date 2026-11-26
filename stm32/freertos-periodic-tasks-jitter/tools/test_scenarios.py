"""Tests of the periodic-task scenarios (the firmware's task set in the model).

Run:  python -m pytest -q tools

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import statistics

import pytest
import scenarios as sc


def test_absolute_keeps_the_rate():
    r = sc.simulate(2000)
    p = sc.periods(r, "absolute")
    assert statistics.mean(p) == pytest.approx(10.0, abs=0.05)
    assert len(sc.releases(r, "absolute")) == pytest.approx(200, abs=1)
    late = sc.lateness(r, "absolute")
    assert max(late) <= 3.0 + 2.0 + 0.1       # at most one burst plus the other task's work


def test_relative_drifts():
    r = sc.simulate(2000)
    p = sc.periods(r, "relative")
    assert min(p) >= 12.0 - 1e-9              # work + 10 ms, never less
    assert statistics.mean(p) > 12.0
    assert sc.lateness(r, "relative")[-1] > 300.0   # far behind the 10 ms grid after 2 s


def test_without_interference_relative_is_regular_but_slow():
    r = sc.simulate(1000, burst_ms=0)
    p = sc.periods(r, "relative")
    assert max(p) - min(p) <= 2.0 + 1e-9      # at most the equal-priority task's 2 ms of work
    assert statistics.mean(p) == pytest.approx(12.0, abs=0.6)


def test_no_work_no_interference_both_exact():
    r = sc.simulate(500, work_ms=0, burst_ms=0)
    assert {round(x, 6) for x in sc.periods(r, "absolute")} == {10.0}
    assert {round(x, 6) for x in sc.periods(r, "relative")} == {10.0}
