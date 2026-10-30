"""Portable, independent verification of generalized-data waveform export."""
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path


def append(runs, length, level):
    if not length:
        return
    if runs and runs[-1][1] == level:
        runs[-1][0] += length
    else:
        runs.append([length, level])


def decode_tzx(path):
    data = path.read_bytes()
    assert data[:10] == b"ZXTape!\x1a\x01\x14"
    pos = 10
    runs = []
    while pos < len(data):
        block = data[pos]
        pos += 1
        if block == 0x30:
            pos += 1 + data[pos]
            continue
        assert block == 0x19, hex(block)
        length, = struct.unpack_from("<I", data, pos)
        end = pos + 4 + length
        assert end <= len(data)
        pause, totp, npp, asp, totd, npd, asd = struct.unpack_from("<HIBBIBB", data, pos + 4)
        assert (pause, totp, npp, asp, npd, asd) == (0, 0, 0, 0, 1, 0)
        assert 0 < totd <= 65536
        pos += 18
        table = []
        for _ in range(256):
            flag, duration = struct.unpack_from("<BH", data, pos)
            assert flag in (2, 3) and duration > 0
            table.append((duration, flag - 2))
            pos += 3
        assert pos + totd == end
        for index in data[pos:end]:
            append(runs, *table[index])
        pos = end
    assert pos == len(data)
    return runs


def decode_wav(path):
    # Convert cumulative sample boundaries to fixed 3.5MHz TZX units.
    with wave.open(str(path), "rb") as wav:
        assert wav.getnchannels() == 2 and wav.getsampwidth() == 2
        rate = wav.getframerate()
        pcm = list(struct.iter_unpack("<hh", wav.readframes(wav.getnframes())))
    runs = []
    start = 0
    while start < len(pcm):
        level = int(pcm[start][0] >= 0)
        end = start + 1
        while end < len(pcm) and int(pcm[end][0] >= 0) == level:
            end += 1
        append(runs, end * 3500000 // rate - start * 3500000 // rate, level)
        start = end
    return runs


def main():
    with tempfile.TemporaryDirectory(prefix="zqloader-tzx-test-") as directory:
        subprocess.run([sys.argv[1], directory], check=True, timeout=30)
        root = Path(directory)
        expected = {
            "levels": [[10, 0], [20, 1], [70, 0], [50, 1], [1, 0]],
            "long-hold": [[140000, 0], [1, 1]],
            "byte-delay": [[n, i % 2] for i, n in enumerate([91] * 8 + [295] + [91] * 7 + [1])],
            "alphabet-boundary": [[1000 + i, i % 2] for i in range(300)] + [[1, 0]],
            "stream-boundary": [[91, i % 2] for i in range(65540)] + [[1, 0]],
            "clock": [[5, 0], [1, 1]],
            "empty": [],
            "empty-data": [],
        }
        for name, want in expected.items():
            got = decode_tzx(root / (name + ".tzx"))
            assert got == want, (name, got[:20], want[:20])
        for rate in (44100, 48000, 96000, 192000):
            stem = "sampled-" + str(rate)
            got = decode_tzx(root / (stem + ".tzx"))
            want = decode_wav(root / (stem + ".wav"))
            assert got == want, (rate, got, want)
    print("TZX waveform tests passed")


if __name__ == "__main__":
    main()
