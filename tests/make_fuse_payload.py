"""Generate a small, copyright-free 48K snapshot for manual Fuse loading tests.

Usage: python tests/make_fuse_payload.py payload.z80
Successful loading shows a deterministic noise screen and a green border.
"""
import random
import struct
import sys
from pathlib import Path

header = bytearray(30)  # Uncompressed Z80 v1, registers zero, interrupts off.
struct.pack_into("<H", header, 6, 0x8000)
struct.pack_into("<H", header, 8, 0xFF00)
header[29] = 1  # IM 1
ram = bytearray(random.Random(538).getrandbits(49152 * 8).to_bytes(49152, "little"))
ram[0x1800:0x1B00] = bytes([0x47]) * 768
# DI; LD A,4; OUT (254),A; JP 0x8005.
ram[0x4000:0x4008] = bytes.fromhex("f3 3e 04 d3 fe c3 05 80")
Path(sys.argv[1]).write_bytes(header + ram)
