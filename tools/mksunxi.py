#!/usr/bin/env python3
"""mksunxi.py - Patch Allwinner eGON boot0 header checksum.

eGON header layout:
  offset 0x00: branch instruction (4 bytes)
  offset 0x04: magic "eGON.BT0" (8 bytes)
  offset 0x0C: checksum (4 bytes) — BROM verifies this
  offset 0x10: spl_size (4 bytes) — BROM loads this many bytes

Checksum = sum of all uint32 words in [0, spl_size),
           with checksum field set to 0x5F0A6C39 during calculation.

Usage: python3 mksunxi.py <nuttx.bin>
"""

import struct
import sys

STAMP_VALUE = 0x5F0A6C39
ALIGN = 16384
CHECKSUM_OFFSET = 0x0C
SPL_SIZE_OFFSET = 0x10
RAM_START = 0x40000000


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <boot0.bin>")
        sys.exit(1)

    path = sys.argv[1]
    with open(path, "rb") as f:
        data = bytearray(f.read())

    magic = data[4:12]
    if magic != b"eGON.BT0":
        print(f"ERROR: eGON magic not found (got {magic!r})")
        sys.exit(1)

    spl_size = struct.unpack_from("<I", data, SPL_SIZE_OFFSET)[0]
    if spl_size > 0x10000000:
        spl_size -= RAM_START
    if spl_size == 0 or spl_size > len(data):
        print(f"WARNING: spl_size=0x{spl_size:x} invalid, using full file size")
        spl_size = len(data)

    aligned_spl = (spl_size + ALIGN - 1) & ~(ALIGN - 1)
    struct.pack_into("<I", data, SPL_SIZE_OFFSET, aligned_spl)

    if len(data) < aligned_spl:
        data.extend(b"\xff" * (aligned_spl - len(data)))

    struct.pack_into("<I", data, CHECKSUM_OFFSET, STAMP_VALUE)

    checksum = 0
    for i in range(0, aligned_spl, 4):
        checksum += struct.unpack_from("<I", data, i)[0]
        checksum &= 0xFFFFFFFF

    struct.pack_into("<I", data, CHECKSUM_OFFSET, checksum)

    with open(path, "wb") as f:
        f.write(data)

    print(
        f"Patched: spl_size={aligned_spl} (0x{aligned_spl:x}), "
        f"total={len(data)}, checksum=0x{checksum:08x}"
    )


if __name__ == "__main__":
    main()
