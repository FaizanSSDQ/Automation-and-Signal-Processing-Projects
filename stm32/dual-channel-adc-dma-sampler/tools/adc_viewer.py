"""
Live viewer for the NUCLEO-L152RE dual-channel ADC + DMA sampler.

Reads the binary telemetry frames sent over the ST-LINK virtual COM port,
validates them (sync + CRC-16/CCITT-FALSE), and plots both channels with
per-block statistics. Press the board's blue button to start/stop raw
sample streaming.

Usage
    python adc_viewer.py --port COM5            # Windows
    python adc_viewer.py --port /dev/ttyACM0    # Linux
    python adc_viewer.py --simulate             # no hardware: synthetic 50 Hz / 120 Hz signals
    python adc_viewer.py --port COM5 --log stats.csv --no-plot

Author : Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
License: MIT
"""

from __future__ import annotations

import argparse
import csv
import math
import struct
import sys
import time
from collections import deque
from collections.abc import Iterator
from dataclasses import dataclass

SYNC = b"\xa5\x5a"
HEADER = struct.Struct("<BHH")  # type, seq, len (after the two sync bytes)
TYPE_STATS = 0x01
TYPE_SAMPLES = 0x02
VREF_MV = 3300
ADC_FULL_SCALE = 4095
BAUD = 230400
CPU_HZ = 32_000_000


def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), identical to frame.c."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode_frame(frame_type: int, seq: int, payload: bytes) -> bytes:
    """Python twin of frame_encode(); used by the simulator and tests."""
    body = HEADER.pack(frame_type, seq & 0xFFFF, len(payload)) + payload
    return SYNC + body + struct.pack("<H", crc16_ccitt(body))


@dataclass(frozen=True)
class Frame:
    type: int
    seq: int
    payload: bytes


@dataclass(frozen=True)
class ChannelStats:
    min: int
    max: int
    mean: int
    ac_rms: int


@dataclass(frozen=True)
class BlockStats:
    sample_rate_hz: int
    frames: int
    channels: tuple[ChannelStats, ChannelStats]
    overruns: int
    tx_drops: int
    cpu_cycles: int

    @property
    def cpu_load_percent(self) -> float:
        block_s = self.frames / self.sample_rate_hz
        return 100.0 * (self.cpu_cycles / CPU_HZ) / block_s


STATS_FORMAT = struct.Struct("<HH" + "HHHH" * 2 + "HHI")


def decode_stats(payload: bytes) -> BlockStats:
    f = STATS_FORMAT.unpack(payload)
    ch = (ChannelStats(*f[2:6]), ChannelStats(*f[6:10]))
    return BlockStats(f[0], f[1], ch, f[10], f[11], f[12])


def decode_samples(payload: bytes, channels: int = 2) -> list[list[int]]:
    """Split interleaved uint16 samples into one list per channel."""
    values = struct.unpack(f"<{len(payload) // 2}H", payload)
    return [list(values[c::channels]) for c in range(channels)]


def counts_to_mv(counts: float) -> float:
    return counts * VREF_MV / ADC_FULL_SCALE


class FrameParser:
    """Incremental parser: feed arbitrary byte chunks, get complete valid frames.

    Corrupted or partial data is skipped by searching for the next sync word,
    so the stream recovers automatically after noise or a mid-frame connect.
    """

    MAX_PAYLOAD = 4096

    def __init__(self) -> None:
        self._buf = bytearray()
        self.crc_errors = 0

    def feed(self, data: bytes) -> Iterator[Frame]:
        self._buf.extend(data)
        while True:
            start = self._buf.find(SYNC)
            if start < 0:
                del self._buf[:-1]  # keep a possible first sync byte
                return
            del self._buf[:start]
            if len(self._buf) < 2 + HEADER.size:
                return
            ftype, seq, length = HEADER.unpack_from(self._buf, 2)
            if length > self.MAX_PAYLOAD:
                del self._buf[:2]
                continue
            total = 2 + HEADER.size + length + 2
            if len(self._buf) < total:
                return
            body = bytes(self._buf[2 : total - 2])
            (crc,) = struct.unpack_from("<H", self._buf, total - 2)
            if crc == crc16_ccitt(body):
                del self._buf[:total]
                yield Frame(ftype, seq, body[HEADER.size :])
            else:
                self.crc_errors += 1
                del self._buf[:2]


# ---- Simulator ------------------------------------------------------------------------------


def simulate_stream(rate_hz: int = 2000, frames_per_block: int = 256) -> Iterator[bytes]:
    """Yield byte chunks exactly like the firmware: samples + stats per block."""
    seq = 0
    t = 0
    while True:
        samples, ch_vals = [], ([], [])
        for _ in range(frames_per_block):
            s = t / rate_hz
            a = 2048 + 1200 * math.sin(2 * math.pi * 50 * s)
            b = 2048 + 600 * math.sin(2 * math.pi * 120 * s) + 300 * math.sin(2 * math.pi * 7 * s)
            for v, store in ((a, ch_vals[0]), (b, ch_vals[1])):
                v = max(0, min(ADC_FULL_SCALE, round(v)))
                samples.append(v)
                store.append(v)
            t += 1
        payload = struct.pack(f"<{len(samples)}H", *samples)
        stats = []
        for vals in ch_vals:
            mean = sum(vals) / len(vals)
            rms = math.sqrt(sum((v - mean) ** 2 for v in vals) / len(vals))
            stats += [min(vals), max(vals), round(mean), round(rms)]
        stats_payload = STATS_FORMAT.pack(rate_hz, frames_per_block, *stats, 0, 0, 41_000)
        yield (encode_frame(TYPE_SAMPLES, seq, payload)
               + encode_frame(TYPE_STATS, seq, stats_payload))
        seq = (seq + 1) & 0xFFFF


# ---- Front end ------------------------------------------------------------------------------


def open_source(args: argparse.Namespace) -> Iterator[bytes]:
    if args.simulate:
        block_s = 256 / 2000
        for chunk in simulate_stream():
            yield chunk
            time.sleep(block_s)
    else:
        import serial  # pyserial, imported lazily so tests do not need it

        with serial.Serial(args.port, BAUD, timeout=0.05) as port:
            while True:
                yield port.read(4096)


def run(args: argparse.Namespace) -> None:
    parser = FrameParser()
    history = [deque(maxlen=args.window), deque(maxlen=args.window)]
    log = None
    log_file = None
    if args.log:
        log_file = open(args.log, "w", newline="")  # noqa: SIM115 (closed in finally)
        log = csv.writer(log_file)
        log.writerow(["seq", "ch1_min", "ch1_max", "ch1_mean", "ch1_rms",
                      "ch2_min", "ch2_max", "ch2_mean", "ch2_rms", "overruns", "tx_drops",
                      "cpu_load_pct"])

    plot = None
    if not args.no_plot:
        import matplotlib.pyplot as plt

        plt.ion()
        fig, ax = plt.subplots(figsize=(10, 5))
        lines = [ax.plot([], [], label=f"CH{c + 1} ({pin})")[0]
                 for c, pin in enumerate(("PA1 / A1", "PA4 / A2"))]
        ax.set(xlabel="sample", ylabel="voltage (mV)", ylim=(0, VREF_MV),
               title="NUCLEO-L152RE dual-channel ADC (2 kHz per channel)")
        ax.grid(alpha=0.3)
        ax.legend(loc="upper right")
        status = ax.text(0.01, 0.02, "", transform=ax.transAxes, family="monospace", fontsize=9)
        plot = (plt, fig, ax, lines, status)

    try:
        _process(args, parser, history, log, plot)
    finally:
        if log_file:
            log_file.close()


def _process(args, parser, history, log, plot) -> None:
    for chunk in open_source(args):
        for frame in parser.feed(chunk):
            if frame.type == TYPE_SAMPLES:
                for c, values in enumerate(decode_samples(frame.payload)):
                    history[c].extend(counts_to_mv(v) for v in values)
            elif frame.type == TYPE_STATS:
                st = decode_stats(frame.payload)
                c1, c2 = st.channels
                text = (f"seq {frame.seq:5d} | CH1 mean {counts_to_mv(c1.mean):6.0f} mV "
                        f"rms {counts_to_mv(c1.ac_rms):5.0f} mV | CH2 mean "
                        f"{counts_to_mv(c2.mean):6.0f} mV rms {counts_to_mv(c2.ac_rms):5.0f} mV | "
                        f"CPU {st.cpu_load_percent:4.1f}% | overruns {st.overruns} "
                        f"drops {st.tx_drops} crc-err {parser.crc_errors}")
                if log:
                    log.writerow([frame.seq, *(v for ch in st.channels for v in
                                               (ch.min, ch.max, ch.mean, ch.ac_rms)),
                                  st.overruns, st.tx_drops, round(st.cpu_load_percent, 2)])
                if plot:
                    plt, fig, ax, lines, status = plot
                    for line, data in zip(lines, history, strict=True):
                        line.set_data(range(len(data)), list(data))
                    ax.set_xlim(0, args.window)
                    status.set_text(text)
                    fig.canvas.draw_idle()
                    plt.pause(0.001)
                else:
                    print(text)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--port", help="serial port of the ST-LINK VCP (e.g. COM5, /dev/ttyACM0)")
    src.add_argument("--simulate", action="store_true", help="use synthetic data, no hardware")
    ap.add_argument("--log", help="write per-block statistics to this CSV file")
    ap.add_argument("--no-plot", action="store_true", help="print statistics only")
    ap.add_argument("--window", type=int, default=1024, help="samples shown per channel")
    try:
        run(ap.parse_args(argv))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
