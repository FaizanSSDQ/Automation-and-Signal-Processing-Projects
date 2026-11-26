"""A small discrete-time model of the FreeRTOS scheduler, for timeline figures
and for checking scheduling behaviour without hardware.

Rules modeled (FreeRTOS with configUSE_PREEMPTION = 1, configUSE_TIME_SLICING = 1):
- The highest-priority ready task runs. A task that becomes ready with a higher
  priority than the running task preempts it immediately.
- Tasks of equal priority share the CPU round-robin, switching on each tick.
- vTaskDelay(n) blocks until the tick count has advanced by n; vTaskDelayUntil
  blocks until a fixed wake time (previous wake + period), so it does not drift.
- Binary semaphores have no owner; mutexes use priority inheritance: while a
  higher-priority task waits, the holder runs at that higher priority.

Each task is a Python generator that yields actions:
    ("run", ms)            use the CPU for ms (preemptible)
    ("delay", ms)          vTaskDelay
    ("delay_until", ms)    vTaskDelayUntil with this period
    ("take", lock)         block until the lock is free, then own it
    ("give", lock)         release it
    ("mark", text)         record an event at the current time

Time is simulated in steps of 0.1 ms; the tick is 1 ms.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from __future__ import annotations

from collections.abc import Callable, Generator
from dataclasses import dataclass, field

STEP = 1          # 0.1 ms
TICK = 10         # 1 ms in steps
STEPS_PER_MS = 10

Program = Callable[["Task"], Generator]


@dataclass
class Lock:
    name: str
    kind: str = "mutex"           # "mutex" (inheritance) or "semaphore"
    owner: Task | None = None
    waiters: list[Task] = field(default_factory=list)


@dataclass
class Task:
    name: str
    base_prio: int
    program: Program
    prio: int = 0
    state: str = "ready"           # ready | blocked | done
    wake_at: int | None = None
    run_left: int = 0
    last_wake: int = 0
    gen: Generator | None = None
    ready_since: int = 0
    pending: bool = True           # woke up and has not run yet
    # statistics
    wait_times: list[int] = field(default_factory=list)   # wake -> first run, in steps
    block_times: list[int] = field(default_factory=list)  # time spent waiting for locks
    block_start: int = 0
    cpu: int = 0


@dataclass
class Segment:
    task: str
    start: float   # ms
    end: float     # ms
    prio: int      # effective priority while running


@dataclass
class Result:
    segments: list[Segment]
    marks: list[tuple[float, str, str]]   # (ms, task, text)
    tasks: dict[str, Task]
    duration_ms: float

    def cpu_share(self, name: str) -> float:
        return self.tasks[name].cpu / (self.duration_ms * STEPS_PER_MS)

    def runs(self, name: str) -> int:
        return sum(1 for s in self.segments if s.task == name)


class Scheduler:
    def __init__(self, time_slicing: bool = True):
        self.time_slicing = time_slicing
        self.tasks: list[Task] = []
        self.now = 0
        self.ready: list[Task] = []     # in arrival order; selection by priority
        self.running: Task | None = None
        self.segments: list[Segment] = []
        self.marks: list[tuple[float, str, str]] = []

    def add(self, name: str, prio: int, program: Program) -> Task:
        t = Task(name=name, base_prio=prio, program=program, prio=prio)
        t.gen = program(t)
        self.tasks.append(t)
        self.ready.append(t)
        return t

    # ---- state changes ------------------------------------------------------------------------

    def _make_ready(self, t: Task) -> None:
        t.state = "ready"
        t.wake_at = None
        t.ready_since = self.now
        t.pending = True
        if t not in self.ready:
            self.ready.append(t)

    def _block(self, t: Task, wake_at: int | None) -> None:
        t.state = "blocked"
        t.wake_at = wake_at
        if t in self.ready:
            self.ready.remove(t)

    def _take(self, t: Task, lock: Lock) -> bool:
        if lock.owner is None:
            lock.owner = t
            return True
        lock.waiters.append(t)
        lock.waiters.sort(key=lambda w: -w.prio)
        t.block_start = self.now
        self._block(t, None)
        if lock.kind == "mutex" and lock.owner.prio < t.prio:
            lock.owner.prio = t.prio            # priority inheritance
        return False

    def _give(self, t: Task, lock: Lock) -> None:
        assert lock.owner is t or lock.kind == "semaphore", f"{t.name} gives {lock.name} it does not own"
        if lock.kind == "mutex":
            t.prio = t.base_prio                # disinherit
        lock.owner = None
        if lock.waiters:
            w = lock.waiters.pop(0)
            lock.owner = w
            w.block_times.append(self.now - w.block_start)
            self._make_ready(w)

    def _advance(self, t: Task) -> None:
        """Execute zero-time actions until the task needs CPU time or blocks."""
        while t.run_left == 0 and t.state == "ready":
            try:
                action = next(t.gen)
            except StopIteration:
                t.state = "done"
                self.ready.remove(t)
                return
            kind = action[0]
            if kind == "run":
                t.run_left = max(1, round(action[1] * STEPS_PER_MS))
            elif kind == "delay":
                ticks = max(1, round(action[1]))
                self._block(t, (self.now // TICK + ticks) * TICK)
            elif kind == "delay_until":
                t.last_wake += round(action[1]) * TICK
                if t.last_wake > self.now:
                    self._block(t, t.last_wake)
            elif kind == "take":
                self._take(t, action[1])
            elif kind == "give":
                self._give(t, action[1])
            elif kind == "mark":
                self.marks.append((self.now / STEPS_PER_MS, t.name, action[1]))
            else:
                raise ValueError(f"unknown action {action!r}")

    def _select(self) -> Task | None:
        if not self.ready:
            return None
        top = max(t.prio for t in self.ready)
        cur = self.running
        if cur is not None and cur in self.ready and cur.prio == top:
            return cur                          # equal priority: no preemption between ticks
        return next(t for t in self.ready if t.prio == top)

    # ---- main loop ------------------------------------------------------------------------

    def run(self, duration_ms: float) -> Result:
        end = round(duration_ms * STEPS_PER_MS)
        seg_task, seg_start, seg_prio = None, 0, 0
        while self.now < end:
            for t in self.tasks:
                if t.state == "blocked" and t.wake_at is not None and self.now >= t.wake_at:
                    self._make_ready(t)

            if self.now % TICK == 0 and self.time_slicing and self.running in self.ready:
                peers = [t for t in self.ready if t.prio == self.running.prio]
                if len(peers) > 1:              # rotate: running task goes to the back
                    self.ready.remove(self.running)
                    self.ready.append(self.running)
                    self.running = None

            # Let tasks without pending CPU work act (they may block, give, take ...).
            for _ in range(len(self.tasks) * 4):
                t = self._select()
                if t is None or t.run_left > 0:
                    break
                self.running = t
                self._advance(t)

            t = self._select()
            if t is not None and t.pending:
                t.wait_times.append(self.now - t.ready_since)
                t.pending = False
            self.running = t

            name = t.name if t else None
            prio = t.prio if t else 0
            if name != seg_task or prio != seg_prio:
                if seg_task is not None:
                    self.segments.append(Segment(seg_task, seg_start / 10, self.now / 10, seg_prio))
                seg_task, seg_start, seg_prio = name, self.now, prio

            if t is not None:
                t.run_left -= STEP
                t.cpu += STEP
            self.now += STEP

        if seg_task is not None:
            self.segments.append(Segment(seg_task, seg_start / 10, self.now / 10, seg_prio))
        return Result(self.segments, self.marks, {t.name: t for t in self.tasks}, duration_ms)


# ---- plotting -------------------------------------------------------------------------------

def plot_timeline(ax, result: Result, order: list[str], colors: dict[str, str], title: str = "",
                  show_prio_change: bool = False, min_width: float = 0.0) -> None:
    """Gantt chart: one row per task, a bar wherever it owns the CPU. Bars shorter than
    min_width (ms) are drawn at that width so very short runs stay visible."""
    for row, name in enumerate(order):
        y = len(order) - 1 - row
        base = result.tasks[name].base_prio
        for s in result.segments:
            if s.task == name:
                boosted = show_prio_change and s.prio != base
                ax.broken_barh([(s.start, max(s.end - s.start, min_width))], (y - 0.35, 0.7), facecolors=colors[name],
                               edgecolor="#c53030" if boosted else "none", linewidth=1.5 if boosted else 0,
                               hatch="///" if boosted else None)
    ax.set_yticks(range(len(order)))
    ax.set_yticklabels(list(reversed(order)))
    ax.set_xlim(0, result.duration_ms)
    ax.set_ylim(-0.6, len(order) - 0.4)
    ax.grid(True, axis="x", alpha=0.3)
    if title:
        ax.set_title(title, fontsize=10, loc="left")
    for side in ("right", "top"):
        ax.spines[side].set_visible(False)
