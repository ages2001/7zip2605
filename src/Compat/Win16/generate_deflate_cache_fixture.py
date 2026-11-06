#!/usr/bin/env python3
"""Make a raw stream with repeated maximum-distance matches, checked by zlib."""
import argparse
from pathlib import Path
import random
import zlib

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
history = random.Random(81).randbytes(32768)
bits = []
def put(value, count):
    bits.extend((value >> i) & 1 for i in range(count))
def code(value, count):
    bits.extend((value >> i) & 1 for i in range(count - 1, -1, -1))
put(3, 3)
for _ in range(128):
    code(0xc5, 8)  # Fixed symbol 285: length 258.
    code(29, 5)
    put(8191, 13)  # Distance 24577 + 8191 = 32768.
code(0, 7)
tail = bytes(sum(bit << j for j, bit in enumerate(bits[i:i + 8]))
             for i in range(0, len(bits), 8))
packed = b'\0\0\x80\xff\x7f' + history + tail
expected = (history * 3)[:32768 + 128 * 258]
assert zlib.decompress(packed, -15) == expected
(a.output / 'D81IN.DEF').write_bytes(packed)
(a.output / 'D81OUT.BIN').write_bytes(expected)
print('PASS independent zlib verification of maximum-distance fixture')
