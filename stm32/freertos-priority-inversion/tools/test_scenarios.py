"""Tests of the priority-inversion scenarios (the firmware's scene in the model).

Run:  python -m pytest -q tools

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import pytest
import scenarios as sc


def test_semaphore_inverts_priorities():
    r = sc.simulate("semaphore", 500)
    waits = sc.wait_per_cycle_ms(r)
    assert len(waits) == 5
    # low still needs 8 ms after high asks, and medium (30 ms) runs first
    assert all(w == pytest.approx(38.0) for w in waits)


def test_mutex_bounds_the_wait():
    r = sc.simulate("mutex", 500)
    assert all(w == pytest.approx(8.0) for w in sc.wait_per_cycle_ms(r))
    # while high waits, low runs at high's priority
    boosted = [s for s in r.segments if s.task == "low" and s.prio == sc.HIGH]
    assert boosted and boosted[0].start == pytest.approx(2.0) and boosted[0].end == pytest.approx(10.0)


@pytest.mark.parametrize("medium", [0, 10, 30, 60])
def test_wait_grows_with_medium_only_for_the_semaphore(medium):
    sem = sc.wait_per_cycle_ms(sc.simulate("semaphore", 200, medium_ms=medium))
    mtx = sc.wait_per_cycle_ms(sc.simulate("mutex", 200, medium_ms=medium))
    assert sem[0] == pytest.approx(8.0 + medium)
    assert mtx[0] == pytest.approx(8.0)


def test_no_wait_when_low_is_done_first():
    r = sc.simulate("semaphore", 300, hold_ms=1)
    assert all(w == pytest.approx(0.0) for w in sc.wait_per_cycle_ms(r))


def test_medium_still_runs_with_the_mutex():
    r = sc.simulate("mutex", 100)
    med = [s for s in r.segments if s.task == "medium"]
    assert med[0].start == pytest.approx(11.0)       # after low gives (10) and high runs (1)
    assert sum(s.end - s.start for s in med) == pytest.approx(30.0)
