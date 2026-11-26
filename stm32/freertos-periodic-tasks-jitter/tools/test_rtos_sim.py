"""Tests of the scheduler model (tools/rtos_sim.py), shared by the FreeRTOS projects.

Run:  python -m pytest -q tools

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import pytest
import rtos_sim as rs

# ---- scheduler rules ------------------------------------------------------------------------


def test_higher_priority_preempts_immediately():
    s = rs.Scheduler()

    def low(t):
        yield ("run", 50)

    def high(t):
        yield ("delay", 10)
        yield ("run", 5)

    s.add("low", 1, low)
    s.add("high", 2, high)
    r = s.run(60)
    hi = [seg for seg in r.segments if seg.task == "high"]
    assert hi[0].start == pytest.approx(10.0) and hi[0].end == pytest.approx(15.0)
    assert r.tasks["high"].wait_times[-1] == 0          # no delay between wake and run
    assert r.tasks["low"].cpu == 500                    # low still gets its 50 ms


def test_equal_priorities_share_by_tick():
    s = rs.Scheduler()

    def spin(t):
        yield ("run", 100)

    s.add("a", 1, spin)
    s.add("b", 1, spin)
    r = s.run(20)
    assert r.cpu_share("a") == pytest.approx(0.5)
    assert all(seg.end - seg.start == pytest.approx(1.0) for seg in r.segments)   # 1 ms slices


def test_no_time_slicing_runs_to_block():
    s = rs.Scheduler(time_slicing=False)

    def spin(t):
        yield ("run", 10)
        yield ("delay", 100)

    s.add("a", 1, spin)
    s.add("b", 1, spin)
    r = s.run(30)
    assert [seg.task for seg in r.segments if seg.task] == ["a", "b"]


def test_delay_drifts_but_delay_until_does_not():
    s = rs.Scheduler()

    def relative(t):
        while True:
            yield ("mark", "release")
            yield ("run", 3)
            yield ("delay", 10)

    def absolute(t):
        while True:
            yield ("mark", "release")
            yield ("run", 3)
            yield ("delay_until", 10)

    s.add("relative", 2, relative)
    s.add("absolute", 1, absolute)
    r = s.run(100)
    rel = [m[0] for m in r.marks if m[1] == "relative"]
    absol = [m[0] for m in r.marks if m[1] == "absolute"]
    assert rel[1] - rel[0] == pytest.approx(13.0)       # period = work + delay
    assert absol[2] - absol[1] == pytest.approx(10.0)   # fixed rate


def test_mutex_inheritance_versus_semaphore():
    def run(kind):
        s = rs.Scheduler()
        lock = rs.Lock("res", kind)

        def low(t):
            yield ("take", lock)
            yield ("run", 10)
            yield ("give", lock)
            yield ("delay", 1000)

        def medium(t):
            yield ("delay", 2)
            yield ("run", 30)
            yield ("delay", 1000)

        def high(t):
            yield ("delay", 1)
            yield ("take", lock)
            yield ("run", 1)
            yield ("give", lock)
            yield ("delay", 1000)

        s.add("low", 1, low)
        s.add("medium", 2, medium)
        s.add("high", 3, high)
        return s.run(60)

    sem = run("semaphore")
    mtx = run("mutex")
    assert sem.tasks["high"].block_times[0] / 10 == pytest.approx(39.0)   # 9 ms + medium's 30 ms
    assert mtx.tasks["high"].block_times[0] / 10 == pytest.approx(9.0)    # only low's remaining work
    boosted = [seg for seg in mtx.segments if seg.task == "low" and seg.prio == 3]
    assert boosted and boosted[0].start == pytest.approx(1.0)
