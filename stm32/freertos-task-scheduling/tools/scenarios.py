"""The firmware's task set (app_sched.c) as simulator scenarios.

    console     high,   ~0.5 ms of work when a command arrives (here at 150 ms)
    worker      configurable priority, busy load % of every 100 ms (vTaskDelayUntil)
    blink_fast  low,    toggles LD2, then vTaskDelay(100)
    blink_slow  low,    toggles PA4, then vTaskDelay(500)

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from __future__ import annotations

import rtos_sim as rs

LOW, BELOW, NORMAL, ABOVE, HIGH = 8, 16, 24, 32, 40
ORDER = ["console", "worker", "blink_fast", "blink_slow"]
COLORS = {"console": "#6b46c1", "worker": "#dd6b20", "blink_fast": "#2b6cb0", "blink_slow": "#2f855a"}


def build(worker_prio: int, load_pct: int, command_at_ms: float = 150.0) -> rs.Scheduler:
    s = rs.Scheduler()

    def console(t):
        yield ("delay", command_at_ms)
        yield ("mark", "command")
        yield ("run", 0.5)
        while True:
            yield ("delay", 1000)

    def worker(t):
        while True:
            if load_pct:
                yield ("run", load_pct)          # load % of a 100 ms period, in ms
            yield ("delay_until", 100)

    def blink(period):
        def body(t):
            while True:
                yield ("run", 0.05)
                yield ("mark", "toggle")
                yield ("delay", period)
        return body

    s.add("console", HIGH, console)
    s.add("worker", worker_prio, worker)
    s.add("blink_fast", LOW, blink(100))
    s.add("blink_slow", LOW, blink(500))
    return s


SCENARIOS = {
    "normal": {"worker_prio": BELOW, "load_pct": 30,
                   "title": "worker 'below', 30 % load: everything gets its turn"},
    "preemption": {"worker_prio": BELOW, "load_pct": 70,
                       "title": "worker 'below', 70 % load: LED tasks only run in the worker's gaps"},
    "starvation": {"worker_prio": ABOVE, "load_pct": 100,
                       "title": "worker 'above', 100 % load: LED tasks starve (B1 demo)"},
    "time_slicing": {"worker_prio": LOW, "load_pct": 100,
                         "title": "worker 'low' (equal to LEDs), 100 % load: round-robin time slicing"},
}


def simulate(name: str, duration_ms: float = 300.0) -> rs.Result:
    cfg = SCENARIOS[name]
    return build(cfg["worker_prio"], cfg["load_pct"]).run(duration_ms)


def toggle_latencies_ms(result: rs.Result, task: str) -> list[float]:
    """Wake-to-run delay of each LED toggle."""
    return [w / rs.STEPS_PER_MS for w in result.tasks[task].wait_times]
