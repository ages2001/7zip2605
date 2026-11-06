#!/usr/bin/env python3
"""Independent large single-stream 7z filter and filter-chain fixtures."""
from pathlib import Path
import argparse
import hashlib
import json
import random
import subprocess
import zlib

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--directory', type=Path, required=True)
p.add_argument('--sevenzip', type=Path, required=True)
a = p.parse_args()
root = a.directory.resolve()
root.mkdir(parents=True, exist_ok=True)
r = random.Random(53)
seed = bytes(r.randrange(256) for _ in range(90000))
# End with an incomplete x86 call. The non-aligned length also exercises the
# literal tails of the 2-, 4- and 16-byte branch filters and filter chains.
data = seed * 4 + b'\xe8\x01\x02'
(root / 'LARGE.BIN').write_bytes(data)
cases = {
    'FBCJ': ['BCJ', 'LZMA2:d=128k'],
    'FARM': ['ARM', 'LZMA2:d=128k'],
    'FARMT': ['ARMT', 'LZMA2:d=128k'],
    'FARM64': ['ARM64', 'LZMA2:d=128k'],
    'FPPC': ['PPC', 'LZMA2:d=128k'],
    'FSPARC': ['SPARC', 'LZMA2:d=128k'],
    'FIA64': ['IA64', 'LZMA2:d=128k'],
    'FRISCV': ['RISCV', 'LZMA2:d=128k'],
    'FDELTA': ['Delta:256', 'LZMA2:d=128k'],
    'FA64PC': ['ARM64:4294967292', 'LZMA:d=128k'],
    'FRVCPC': ['RISCV:4294967294', 'LZMA:d=128k'],
    'FCHAIN2': ['Delta:4', 'BCJ', 'LZMA:d=128k'],
    'FCHAIN3': ['Delta:1', 'Delta:4', 'BCJ', 'LZMA2:d=128k'],
    'FCOPY': ['BCJ', 'Copy'],
    'FDFLT': ['Delta:4', 'Deflate'],
}
manifest = {'size': len(data), 'crc32': '%08x' % zlib.crc32(data),
            'sha256': hashlib.sha256(data).hexdigest(), 'archives': {}}
for stem, methods in cases.items():
    archive = root / (stem + '.7Z')
    if archive.exists():
        raise FileExistsError(archive)
    command = [str(a.sevenzip.resolve()), 'a', '-t7z', '-mhc=off', '-mmt=off',
               *('-m%d=%s' % (i, method) for i, method in enumerate(methods)),
               str(archive), 'LARGE.BIN']
    subprocess.run(command, cwd=root, check=True, stdout=subprocess.DEVNULL)
    decoded = subprocess.check_output([str(a.sevenzip.resolve()), 'x', '-so', str(archive)])
    assert decoded == data, archive
    listing = subprocess.check_output([str(a.sevenzip.resolve()), 'l', '-slt', str(archive)])
    (root / (stem + '.slt')).write_bytes(listing)
    manifest['archives'][archive.name] = {
        'methods': methods, 'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
        'independent_host_decode': True,
    }
    print(archive.name, archive.stat().st_size, flush=True)
(root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
damaged = bytearray((root / 'FCOPY.7Z').read_bytes())
damaged[32 + 40000] ^= 0x80
(root / 'FBAD.7Z').write_bytes(damaged)
