"""Command-line SUMP client for the STM32 logic analyzer.

Captures over the ST-LINK virtual COM port and saves the result as a VCD file
(open it in PulseView or GTKWave) and/or a PNG plot. PulseView can also talk
to the board directly; this script is for scripted tests and for using the
edge-trigger extension, which PulseView does not send.

Examples:
    python sump_capture.py COM5 --info
    python sump_capture.py COM5 --rate 1M --samples 4096 --vcd capture.vcd --png capture.png
    python sump_capture.py /dev/ttyACM0 --rate 100k --trigger 7:f --pre 10 --png uart.png
    python sump_capture.py sim --rate 100k --trigger 3:f --pre 4 --png demo.png

The port name "sim" runs the host simulator (build/sim_device from "make sim"):
the firmware's SUMP and capture code with modeled demo signals as inputs.

Trigger syntax: comma-separated CHANNEL:CONDITION with 0 (low), 1 (high),
r (rising edge) or f (falling edge), e.g. "0:1,3:r". All conditions must hold
on the same sample.

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

SUMP_CLOCK_HZ = 100_000_000
TIMER_CLOCK_HZ = 64_000_000  # firmware: TIM2 kernel clock
MIN_TICKS = 16  # firmware: fastest rate 4 MS/s
NUM_CHANNELS = 8

CMD_RESET, CMD_RUN, CMD_ID, CMD_METADATA = 0x00, 0x01, 0x02, 0x04
CMD_DIVIDER, CMD_READ_DELAY, CMD_FLAGS = 0x80, 0x81, 0x82
CMD_TRIG_MASK, CMD_TRIG_VALUE, CMD_TRIG_CONFIG, CMD_TRIG_EDGE = 0xC0, 0xC1, 0xC2, 0xC3

# Flags: disable channel groups 2..4 so the device sends one byte per sample.
FLAGS_8_CHANNELS = 0b1110 << 2


@dataclass
class Trigger:
    level_mask: int = 0
    level_value: int = 0
    rise_mask: int = 0
    fall_mask: int = 0

    @property
    def uses_edges(self) -> bool:
        return bool(self.rise_mask or self.fall_mask)


def parse_rate(text: str) -> float:
    """'1M' -> 1e6, '250k' -> 250e3, '500' -> 500."""
    text = text.strip().lower().removesuffix("hz").removesuffix("s/s")
    scale = {"k": 1e3, "m": 1e6}.get(text[-1:], 1.0)
    if scale != 1.0:
        text = text[:-1]
    rate = float(text) * scale
    if rate <= 0:
        raise ValueError("sample rate must be positive")
    return rate


def divider_for(rate_hz: float) -> int:
    """SUMP divider for a sample rate: rate = 100 MHz / (divider + 1)."""
    return max(0, min(0xFFFFFF, round(SUMP_CLOCK_HZ / rate_hz) - 1))


def timer_ticks(divider: int) -> int:
    """Timer ticks per sample exactly as the firmware computes them
    (la_divider_to_ticks followed by la_timer_split)."""
    ticks = (TIMER_CLOCK_HZ * (divider + 1) + SUMP_CLOCK_HZ // 2) // SUMP_CLOCK_HZ
    ticks = max(ticks, MIN_TICKS)
    div = min((ticks + 65535) // 65536, 65536)
    period = min(max((ticks + div // 2) // div, 1), 65536)
    return div * period


def actual_rate(divider: int) -> float:
    return TIMER_CLOCK_HZ / timer_ticks(divider)


def parse_trigger(spec: str | None) -> Trigger:
    t = Trigger()
    if not spec:
        return t
    for item in spec.split(","):
        ch_text, _, cond = item.strip().partition(":")
        ch = int(ch_text)
        if not 0 <= ch < NUM_CHANNELS:
            raise ValueError(f"channel {ch} out of range 0..{NUM_CHANNELS - 1}")
        bit = 1 << ch
        cond = cond.lower()
        if cond == "1":
            t.level_mask |= bit
            t.level_value |= bit
        elif cond == "0":
            t.level_mask |= bit
        elif cond == "r":
            t.rise_mask |= bit
        elif cond == "f":
            t.fall_mask |= bit
        else:
            raise ValueError(f"bad trigger condition {item!r}; use 0, 1, r or f")
    return t


def long_cmd(cmd: int, value: int) -> bytes:
    return bytes([cmd]) + (value & 0xFFFFFFFF).to_bytes(4, "little")


def build_config(divider: int, samples: int, post: int, trig: Trigger) -> bytes:
    """Commands that configure one capture (everything except RUN)."""
    if samples % 4 or post % 4 or not 4 <= samples <= 0x40000 or not 4 <= post <= samples:
        raise ValueError("samples and post-trigger count must be multiples of 4, 4 <= post <= samples")
    counts = ((samples // 4 - 1) & 0xFFFF) | (((post // 4 - 1) & 0xFFFF) << 16)
    out = long_cmd(CMD_DIVIDER, divider)
    out += long_cmd(CMD_READ_DELAY, counts)
    out += long_cmd(CMD_FLAGS, FLAGS_8_CHANNELS)
    out += long_cmd(CMD_TRIG_MASK, trig.level_mask)
    out += long_cmd(CMD_TRIG_VALUE, trig.level_value)
    out += long_cmd(CMD_TRIG_CONFIG, 1 << 27)  # stage 0, start capture on match
    if trig.uses_edges:
        out += long_cmd(CMD_TRIG_EDGE, trig.rise_mask | (trig.fall_mask << 8))
    return out


def parse_metadata(data: bytes) -> dict:
    """Decode a SUMP metadata reply into a dict."""
    names = {0x01: "device", 0x02: "firmware", 0x20: "probes", 0x21: "sample_memory",
             0x22: "dynamic_memory", 0x23: "max_rate", 0x24: "protocol"}
    meta, i = {}, 0
    while i < len(data):
        key = data[i]
        i += 1
        if key == 0x00:
            break
        kind = key >> 5
        if kind == 0:  # null-terminated string
            end = data.index(0, i)
            value = data[i:end].decode("ascii", "replace")
            i = end + 1
        elif kind == 1:  # 32-bit big-endian
            value = int.from_bytes(data[i:i + 4], "big")
            i += 4
        else:  # 8-bit
            value = data[i]
            i += 1
        meta[names.get(key, f"0x{key:02x}")] = value
    return meta


def find_simulator() -> Path:
    build = Path(__file__).resolve().parent.parent / "build"
    for name in ("sim_device", "sim_device.exe"):
        if (build / name).is_file():
            return build / name
    raise FileNotFoundError(f"simulator not found in {build}; run 'make sim' first")


class SimPort:
    """Port-like wrapper around the host simulator's stdin/stdout."""

    def __init__(self, exe: Path | None = None):
        self.proc = subprocess.Popen([str(exe or find_simulator())], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    def write(self, data: bytes) -> None:
        self.proc.stdin.write(data)
        self.proc.stdin.flush()

    def read(self, n: int) -> bytes:
        return self.proc.stdout.read(n)

    def reset_input_buffer(self) -> None:
        pass

    def close(self) -> None:
        self.proc.stdin.close()
        self.proc.wait(timeout=10)
        self.proc.stdout.close()


class Analyzer:
    """SUMP device on a serial port (anything with read/write/reset_input_buffer)."""

    def __init__(self, port):
        self.port = port

    @classmethod
    def open(cls, name: str, baud: int = 115200, timeout: float = 1.0) -> Analyzer:
        if name == "sim":
            return cls(SimPort())
        import serial  # pyserial

        return cls(serial.Serial(name, baud, timeout=timeout))

    def reset(self) -> None:
        self.port.write(bytes([CMD_RESET] * 5))
        time.sleep(0.05)
        self.port.reset_input_buffer()

    def identify(self) -> str:
        self.port.write(bytes([CMD_ID]))
        return self.port.read(4).decode("ascii", "replace")

    def metadata(self) -> dict:
        self.port.write(bytes([CMD_METADATA]))
        data = bytearray()
        while True:
            b = self.port.read(1)
            if not b:
                break
            data += b
            if b == b"\x00" and _metadata_complete(bytes(data)):
                break
        return parse_metadata(bytes(data))

    def capture(self, divider: int, samples: int, post: int, trig: Trigger,
                timeout_s: float = 10.0) -> list[int]:
        """Run one capture; returns samples oldest first."""
        self.reset()
        self.port.write(build_config(divider, samples, post, trig) + bytes([CMD_RUN]))
        data = bytearray()
        deadline = time.monotonic() + timeout_s
        while len(data) < samples:
            chunk = self.port.read(samples - len(data))
            if chunk:
                data += chunk
                deadline = time.monotonic() + 2.0  # data is flowing
            elif time.monotonic() > deadline:
                raise TimeoutError(f"got {len(data)} of {samples} samples (trigger not seen?)")
        return list(reversed(data))  # SUMP sends the newest sample first


def _metadata_complete(data: bytes) -> bool:
    """True if data ends exactly at the 0x00 end-of-metadata key."""
    i = 0
    while i < len(data):
        key = data[i]
        i += 1
        if key == 0x00:
            return i == len(data)
        kind = key >> 5
        if kind == 0:
            try:
                i = data.index(0, i) + 1
            except ValueError:
                return False
        elif kind == 1:
            i += 4
        else:
            i += 1
    return False


def decode_uart(samples: list[int], channel: int, rate_hz: float, baud: float) -> list[tuple[int, int]]:
    """Decode 8N1 UART frames on one channel. Returns (start_index, byte) per frame
    with a valid start and stop bit; bits are read at their centres."""
    spb = rate_hz / baud
    if spb < 3:
        raise ValueError("need at least 3 samples per bit to decode UART")
    bits = [(s >> channel) & 1 for s in samples]
    frames, i = [], 1
    while i < len(bits):
        if bits[i - 1] == 1 and bits[i] == 0:
            centre = [int(i + (k + 0.5) * spb) for k in range(10)]
            if centre[9] >= len(bits):
                break
            if bits[centre[0]] == 0 and bits[centre[9]] == 1:
                frames.append((i, sum(bits[centre[k + 1]] << k for k in range(8))))
                i = centre[9]
                continue
        i += 1
    return frames


def to_vcd(samples: list[int], rate_hz: float, names: list[str] | None = None) -> str:
    """Value change dump with one wire per channel, timescale 1 ns."""
    names = names or [f"CH{i}" for i in range(NUM_CHANNELS)]
    ids = [chr(33 + i) for i in range(NUM_CHANNELS)]
    lines = ["$timescale 1 ns $end", "$scope module logic $end"]
    lines += [f"$var wire 1 {ids[i]} {names[i]} $end" for i in range(NUM_CHANNELS)]
    lines += ["$upscope $end", "$enddefinitions $end"]
    prev = None
    for n, s in enumerate(samples):
        if s == prev:
            continue
        changes = [f"{(s >> i) & 1}{ids[i]}" for i in range(NUM_CHANNELS)
                   if prev is None or ((s ^ prev) >> i) & 1]
        lines.append(f"#{round(n * 1e9 / rate_hz)}")
        lines += changes
        prev = s
    lines.append(f"#{round(len(samples) * 1e9 / rate_hz)}")
    return "\n".join(lines) + "\n"


def plot(samples: list[int], rate_hz: float, trigger_index: int | None, path: str,
         channels: list[int] | None = None, title: str = "") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np

    channels = channels if channels is not None else list(range(NUM_CHANNELS))
    data = np.array(samples, dtype=np.uint8)
    t_ms = (np.arange(len(data) + 1) - (trigger_index or 0)) * 1e3 / rate_hz
    fig, ax = plt.subplots(figsize=(11, 0.6 * len(channels) + 1.2))
    for row, ch in enumerate(channels):
        bits = (data >> ch) & 1
        y = len(channels) - 1 - row
        ax.stairs(bits * 0.7 + y, t_ms, baseline=None, color=f"C{ch % 10}", linewidth=1.3)
        ax.text(t_ms[0], y + 0.35, f"CH{ch} ", ha="right", va="center", fontsize=9)
    if trigger_index is not None:
        ax.axvline(0, color="red", linestyle="--", linewidth=1)
    ax.set_yticks([])
    ax.set_xlabel("time relative to trigger (ms)" if trigger_index is not None else "time (ms)")
    ax.set_xlim(t_ms[0], t_ms[-1])
    ax.set_title(title or f"{len(data)} samples at {rate_hz / 1e3:g} kS/s")
    for side in ("left", "right", "top"):
        ax.spines[side].set_visible(False)
    fig.tight_layout()
    fig.savefig(path, dpi=130)
    plt.close(fig)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("port", help="serial port (COM5, /dev/ttyACM0) or 'sim' for the host simulator")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--info", action="store_true", help="print device ID and metadata and exit")
    ap.add_argument("--rate", default="1M", help="sample rate, e.g. 4M, 250k, 1000 (default 1M)")
    ap.add_argument("--samples", type=int, default=4096, help="samples to capture (multiple of 4, max 8192)")
    ap.add_argument("--trigger", help='e.g. "7:f" or "0:1,3:r" (default: start immediately)')
    ap.add_argument("--pre", type=float, default=10.0, help="percent of samples before the trigger")
    ap.add_argument("--timeout", type=float, default=10.0, help="seconds to wait for the trigger")
    ap.add_argument("--vcd", help="write a VCD file")
    ap.add_argument("--png", help="write a PNG plot")
    ap.add_argument("--uart", metavar="CH:BAUD", help="decode 8N1 UART on a channel, e.g. 3:9600")
    args = ap.parse_args(argv)

    la = Analyzer.open(args.port, args.baud)
    la.reset()
    ident = la.identify()
    if ident != "1ALS":
        print(f"unexpected ID {ident!r}; is this the logic analyzer firmware?", file=sys.stderr)
        return 1
    if args.info:
        for key, value in la.metadata().items():
            print(f"{key:>14}: {value}")
        return 0

    trig = parse_trigger(args.trigger)
    divider = divider_for(parse_rate(args.rate))
    rate = actual_rate(divider)
    samples = args.samples - args.samples % 4
    pre = int(samples * args.pre / 100) // 4 * 4 if args.trigger else 0
    post = max(4, samples - pre)
    print(f"capturing {samples} samples at {rate / 1e3:g} kS/s, trigger {args.trigger or 'immediate'}")
    data = la.capture(divider, samples, post, trig, args.timeout)
    trigger_index = samples - post if args.trigger else None

    if args.uart:
        ch, _, baud = args.uart.partition(":")
        text = bytes(b for _, b in decode_uart(data, int(ch), rate, float(baud)))
        print(f"UART CH{ch}: {text!r}")
    if args.vcd:
        with open(args.vcd, "w", newline="\n") as f:
            f.write(to_vcd(data, rate))
        print(f"wrote {args.vcd}")
    if args.png:
        plot(data, rate, trigger_index, args.png)
        print(f"wrote {args.png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
