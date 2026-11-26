"""The firmware's task set (app_periodic.c) as simulator scenarios.

    interference  above normal  busy `burst` ms every 37 ms (vTaskDelayUntil)
    relative      normal        release, `work` ms busy, vTaskDelay(10)
    absolute      normal        release, `work` ms busy, vTaskDelayUntil(10)

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from __future__ import annotations

import itertools

import rtos_sim as rs

NORMAL, ABOVE = 24, 32
PERIOD_MS = 10.0
INTERFERENCE_MS = 37
ORDER = ["interference", "relative", "absolute"]
COLORS = {"interference": "#c53030", "relative": "#dd6b20", "absolute": "#2b6cb0"}


def build(work_ms: float = 2.0, burst_ms: float = 3.0) -> rs.Scheduler:
    s = rs.Scheduler()

    def interference(t):
        while True:
            if burst_ms:
                yield ("run", burst_ms)
            yield ("delay_until", INTERFERENCE_MS)

    def relative(t):
        while True:
            yield ("mark", "release")
            if work_ms:
                yield ("run", work_ms)
            yield ("delay", PERIOD_MS)

    def absolute(t):
        while True:
            yield ("mark", "release")
            if work_ms:
                yield ("run", work_ms)
            yield ("delay_until", PERIOD_MS)

    s.add("interference", ABOVE, interference)
    s.add("relative", NORMAL, relative)
    s.add("absolute", NORMAL, absolute)
    return s


def simulate(duration_ms: float = 1000.0, **kw) -> rs.Result:
    return build(**kw).run(duration_ms)


def releases(result: rs.Result, task: str) -> list[float]:
    return [t for t, name, what in result.marks if name == task and what == "release"]


def periods(result: rs.Result, task: str) -> list[float]:
    r = releases(result, task)
    return [b - a for a, b in itertools.pairwise(r)]


def lateness(result: rs.Result, task: str) -> list[float]:
    """Release time minus the ideal fixed-rate schedule (first release + k x 10 ms)."""
    r = releases(result, task)
    return [t - (r[0] + k * PERIOD_MS) for k, t in enumerate(r)]
