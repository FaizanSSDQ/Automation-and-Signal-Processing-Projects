"""The firmware's priority-inversion scene (app_inversion.c) as simulator scenarios.

Every 100 ms:
    t = 0   low    (below normal) takes the lock, works `hold` ms, gives it back
    t = 2   high   (above normal) requests the lock, works 1 ms, gives it back
    t = 3   medium (normal)       runs `medium` ms of CPU, needs no lock

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from __future__ import annotations

import rtos_sim as rs

LOW, NORMAL, HIGH = 16, 24, 32
CYCLE_MS = 100
HIGH_OFFSET_MS = 2
MEDIUM_OFFSET_MS = 3
ORDER = ["high", "medium", "low"]
COLORS = {"high": "#c53030", "medium": "#dd6b20", "low": "#2b6cb0"}


def build(kind: str, hold_ms: float = 10.0, medium_ms: float = 30.0) -> rs.Scheduler:
    """kind: 'semaphore' (binary semaphore, no inheritance) or 'mutex' (priority inheritance)."""
    s = rs.Scheduler()
    lock = rs.Lock("shared", kind)

    def low(t):
        while True:
            yield ("take", lock)
            yield ("run", hold_ms)
            yield ("give", lock)
            yield ("delay_until", CYCLE_MS)

    def high(t):
        yield ("delay_until", HIGH_OFFSET_MS)
        while True:
            yield ("mark", "request")
            yield ("take", lock)
            yield ("mark", "acquired")
            yield ("run", 1)
            yield ("give", lock)
            yield ("delay_until", CYCLE_MS)

    def medium(t):
        yield ("delay_until", MEDIUM_OFFSET_MS)
        while True:
            if medium_ms:
                yield ("run", medium_ms)
            yield ("delay_until", CYCLE_MS)

    s.add("high", HIGH, high)
    s.add("medium", NORMAL, medium)
    s.add("low", LOW, low)
    return s


def simulate(kind: str, duration_ms: float = 100.0, **kw) -> rs.Result:
    return build(kind, **kw).run(duration_ms)


def high_wait_ms(result: rs.Result) -> list[float]:
    """Time high spent blocked on the lock, per cycle in which it had to wait."""
    return [b / rs.STEPS_PER_MS for b in result.tasks["high"].block_times]


def wait_per_cycle_ms(result: rs.Result) -> list[float]:
    """Request-to-acquire time of every cycle (0 when the lock was free)."""
    req = [t for t, task, what in result.marks if task == "high" and what == "request"]
    acq = [t for t, task, what in result.marks if task == "high" and what == "acquired"]
    return [a - r for r, a in zip(req, acq, strict=False)]
