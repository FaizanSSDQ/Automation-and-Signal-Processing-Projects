"""Tests for the SUMP client: command encoding (checked against the same byte
vectors as the firmware tests), rate arithmetic, trigger parsing, metadata
decoding, VCD output and sample order.

Run:  python -m pytest -q tools

Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
"""

import pytest
import sump_capture as sc


class FakePort:
    """Serial stand-in: records writes, replays a queued reply."""

    def __init__(self, reply: bytes = b""):
        self.written = bytearray()
        self.reply = bytearray(reply)

    def write(self, data: bytes) -> None:
        self.written += data

    def read(self, n: int) -> bytes:
        out, self.reply = bytes(self.reply[:n]), self.reply[n:]
        return out

    def reset_input_buffer(self) -> None:
        pass


def firmware_metadata() -> bytes:
    """Byte-for-byte what sump_metadata() in sump.c produces."""
    def u32(key, v):
        return bytes([key]) + v.to_bytes(4, "big")
    return (b"\x01STM32F103 Logic Analyzer\x00" + b"\x021.0.0\x00" + u32(0x20, 8)
            + u32(0x21, 8192) + u32(0x23, 4_000_000) + u32(0x24, 2) + b"\x00")


def test_rate_parsing_and_divider():
    assert sc.parse_rate("1M") == 1e6
    assert sc.parse_rate("250k") == 250e3
    assert sc.parse_rate("10kHz") == 10e3
    assert sc.divider_for(1e6) == 99
    assert sc.divider_for(10e3) == 9999
    with pytest.raises(ValueError):
        sc.parse_rate("0")


def test_actual_rate_matches_firmware_timer():
    assert sc.timer_ticks(99) == 64          # firmware test: la_divider_to_ticks(99) == 64
    assert sc.timer_ticks(24) == 16
    assert sc.timer_ticks(0) == 16           # clamped to 4 MS/s
    assert sc.actual_rate(99) == pytest.approx(1e6)
    assert sc.actual_rate(sc.divider_for(10)) == pytest.approx(10, rel=1e-4)
    # Rates that 64 MHz cannot divide exactly are rounded to the nearest tick.
    assert sc.actual_rate(sc.divider_for(3e6)) == pytest.approx(64e6 / 21)


def test_trigger_parsing():
    t = sc.parse_trigger("0:1,3:0,5:r,7:f")
    assert (t.level_mask, t.level_value, t.rise_mask, t.fall_mask) == (0x09, 0x01, 0x20, 0x80)
    assert t.uses_edges
    assert not sc.parse_trigger(None).uses_edges
    for bad in ("8:1", "1:x", "a:1"):
        with pytest.raises(ValueError):
            sc.parse_trigger(bad)


def test_config_bytes_match_firmware_vectors():
    t = sc.Trigger(level_mask=0x81, level_value=0xFF)
    cmds = sc.build_config(9999, 2048, 1536, t)
    assert cmds[0:5] == bytes([0x80, 0x0F, 0x27, 0x00, 0x00])   # divider 9999
    assert cmds[5:10] == bytes([0x81, 0xFF, 0x01, 0x7F, 0x01])  # 2048 read, 1536 delay
    assert bytes([0xC0, 0x81, 0, 0, 0]) in cmds
    assert 0xC3 not in cmds[::5]                                # no edge extension for levels

    edge = sc.build_config(99, 1024, 512, sc.parse_trigger("2:r,3:f"))
    assert edge[-5:] == bytes([0xC3, 0x04, 0x08, 0x00, 0x00])

    with pytest.raises(ValueError):
        sc.build_config(99, 1000, 2000, t)
    with pytest.raises(ValueError):
        sc.build_config(99, 1022, 512, t)


def test_metadata_decoding():
    meta = sc.parse_metadata(firmware_metadata())
    assert meta["device"] == "STM32F103 Logic Analyzer"
    assert meta["probes"] == 8
    assert meta["sample_memory"] == 8192
    assert meta["max_rate"] == 4_000_000
    assert sc._metadata_complete(firmware_metadata())
    assert not sc._metadata_complete(firmware_metadata()[:-1])


def test_identify_and_metadata_over_port():
    la = sc.Analyzer(FakePort(b"1ALS"))
    assert la.identify() == "1ALS"
    la = sc.Analyzer(FakePort(firmware_metadata() + b"junk"))
    assert la.metadata()["firmware"] == "1.0.0"


def test_capture_reverses_sample_order():
    newest_first = bytes(range(15, -1, -1))
    port = FakePort(newest_first)
    data = sc.Analyzer(port).capture(99, 16, 16, sc.Trigger(), timeout_s=1)
    assert data == list(range(16))
    assert port.written[:5] == bytes(5)          # five resets first
    assert port.written[-1] == sc.CMD_RUN        # run last


def test_capture_times_out_without_data():
    with pytest.raises(TimeoutError):
        sc.Analyzer(FakePort(b"\x01\x02")).capture(99, 16, 16, sc.Trigger(), timeout_s=0.05)


def test_vcd_output():
    vcd = sc.to_vcd([0x00, 0x00, 0x01, 0x03, 0x02], 1e6)
    assert "$enddefinitions $end" in vcd
    body = vcd.split("$enddefinitions $end\n")[1].split("\n")
    assert body[0] == "#0" and body[1:9] == [f"0{chr(33 + i)}" for i in range(8)]
    assert "#2000" in body and "1!" in body           # CH0 rises at 2 us
    assert body[body.index("#3000") + 1] == '1"'      # only CH1 changes at 3 us
    assert body[-2] == "#5000"


def test_plot_writes_png(tmp_path):
    pytest.importorskip("matplotlib")
    out = tmp_path / "cap.png"
    sc.plot([i & 0xFF for i in range(200)], 1e6, 50, str(out), channels=[0, 1, 7])
    assert out.stat().st_size > 1000
