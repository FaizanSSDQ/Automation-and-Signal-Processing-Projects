"""Tests for the host-side frame parser (run: python -m pytest tools)."""

import struct

from adc_viewer import (
    TYPE_SAMPLES,
    TYPE_STATS,
    FrameParser,
    crc16_ccitt,
    decode_samples,
    decode_stats,
    encode_frame,
    simulate_stream,
)

# Produced by the firmware's frame_encode() (C) for a known STATS payload:
# rate 2000, 256 frames, CH1 {100, 4000, 2048, 707}, CH2 {0, 4095, 1024, 12},
# overruns 3, tx_drops 1, cpu_cycles 123456, seq 0x0102.
C_REFERENCE_FRAME = bytes.fromhex(
    "a55a0102011c00d00700016400a00f0008c3020000ff0f00040c000300010040e201003a56"
)


def test_crc_check_value():
    assert crc16_ccitt(b"123456789") == 0x29B1


def test_python_decoder_matches_c_encoder():
    frames = list(FrameParser().feed(C_REFERENCE_FRAME))
    assert len(frames) == 1
    frame = frames[0]
    assert (frame.type, frame.seq) == (TYPE_STATS, 0x0102)
    stats = decode_stats(frame.payload)
    assert (stats.sample_rate_hz, stats.frames) == (2000, 256)
    assert stats.channels[0].ac_rms == 707
    assert stats.channels[1].max == 4095
    assert (stats.overruns, stats.tx_drops, stats.cpu_cycles) == (3, 1, 123456)


def test_python_encoder_matches_c_encoder():
    payload = C_REFERENCE_FRAME[7:-2]
    assert encode_frame(TYPE_STATS, 0x0102, payload) == C_REFERENCE_FRAME


def test_parser_handles_split_chunks_and_garbage():
    stream = b"\x00\xff\xa5" + C_REFERENCE_FRAME + b"noise" + C_REFERENCE_FRAME
    parser = FrameParser()
    frames = []
    for i in range(0, len(stream), 5):  # deliver in tiny pieces
        frames += list(parser.feed(stream[i : i + 5]))
    assert len(frames) == 2
    assert parser.crc_errors == 0


def test_parser_rejects_corrupted_frame():
    bad = bytearray(C_REFERENCE_FRAME)
    bad[10] ^= 0xFF
    parser = FrameParser()
    assert list(parser.feed(bytes(bad) + C_REFERENCE_FRAME)) != []
    assert parser.crc_errors == 1


def test_decode_samples_deinterleaves():
    payload = struct.pack("<6H", 1, 10, 2, 20, 3, 30)
    assert decode_samples(payload) == [[1, 2, 3], [10, 20, 30]]


def test_simulator_produces_valid_blocks():
    chunk = next(simulate_stream())
    frames = list(FrameParser().feed(chunk))
    assert [f.type for f in frames] == [TYPE_SAMPLES, TYPE_STATS]
    ch1, ch2 = decode_samples(frames[0].payload)
    assert len(ch1) == len(ch2) == 256
    stats = decode_stats(frames[1].payload)
    assert 800 < stats.channels[0].ac_rms < 900  # 1200 / sqrt(2) = 849
    assert stats.cpu_load_percent > 0
