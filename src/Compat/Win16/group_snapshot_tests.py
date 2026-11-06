"""Check an actual group snapshot and malformed PMCC bounds with sanitizers."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe', type=Path, required=True)
p.add_argument('--group', type=Path, required=True)
a = p.parse_args()
probe = a.probe.resolve()
original = a.group.read_bytes()
def checksum(data):
    struct.pack_into('<H', data, 4, 0)
    padded = data + b'\0' if len(data) & 1 else data
    total = sum(struct.unpack('<'+'H'*(len(padded)//2), padded))
    struct.pack_into('<H', data, 4, (-total)&65535)
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    def check(name, data, expected):
        path = root/name
        path.write_bytes(data)
        subprocess.run([str(probe), str(path), str(expected)], check=True)
    check('valid.grp', original, 1)
    damaged = bytearray(original);damaged[-1] ^= 1
    check('checksum.grp', damaged, -1)
    check('truncated.grp', original[:33], -1)
    for name, offset, value in [('size', 6, 33), ('slots', 32, 65535),
                                ('item', 34, 65535), ('title', 22, 65535)]:
        damaged = bytearray(original)
        struct.pack_into('<H', damaged, offset, value);checksum(damaged)
        check(name+'.grp', damaged, -1)
    damaged = bytearray(original)
    struct.pack_into('<H', damaged, 22, len(damaged)-1)
    damaged[-1] = ord('X');checksum(damaged)
    check('unterminated.grp', damaged, -1)
    print('PASS valid native group, checksum, truncation, bounded offsets and strings')
