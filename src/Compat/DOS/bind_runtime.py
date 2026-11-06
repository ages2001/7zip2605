"""Bind an unmodified CWSDSTUB to DJGPP COFF, producing a standalone EXE."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--exe', required=True, type=Path)
p.add_argument('--stub', required=True, type=Path)
p.add_argument('--output', required=True, type=Path)
a = p.parse_args()
data = a.exe.read_bytes()
stub = a.stub.read_bytes()
assert data[:2] == stub[:2] == b'MZ'
last, pages = struct.unpack_from('<HH', data, 2)
start = pages * 512 - (512 - last if last else 0)
coff = data[start:]
assert coff[:2] == b'\x4c\x01', 'Expected i386 COFF after the original DOS stub'
last, pages = struct.unpack_from('<HH', stub, 2)
assert pages * 512 - (512-last if last else 0) == len(stub)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_bytes(stub + coff)
sha = lambda b: hashlib.sha256(b).hexdigest()
a.output.with_suffix('.json').write_text(json.dumps({
    'original_exe_sha256': sha(data), 'unmodified_stub_sha256': sha(stub),
    'coff_sha256': sha(coff), 'standalone_exe_sha256': sha(stub+coff),
}, indent=2) + '\n')
