"""Append checked, self-contained payloads to the native DOS setup executable."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--stub', required=True, type=Path)
p.add_argument('--payload', required=True, type=Path)
p.add_argument('--output', required=True, type=Path)
a = p.parse_args()
image = bytearray(a.stub.read_bytes())
assert image[:2] == b'MZ'
records, manifest, names = [], {}, set()
for path in sorted(a.payload.iterdir()):
    if not path.is_file():
        raise SystemExit('Payload must contain only files')
    name = path.name.upper()
    if not re.fullmatch(r'[A-Z0-9_-]{1,8}(?:\.[A-Z0-9_-]{1,3})?', name) or name in names:
        raise SystemExit('Invalid or duplicate DOS filename: '+name)
    stem = name.split('.')[0]
    if stem in {'CON', 'PRN', 'AUX', 'NUL'} or re.fullmatch(r'(COM|LPT)[0-9]', stem):
        raise SystemExit('Reserved DOS filename: '+name)
    names.add(name)
    data = path.read_bytes()
    records.append(struct.pack('<13s3xIII', name.encode(), len(image), len(data), zlib.crc32(data)))
    image.extend(data)
    manifest[name] = hashlib.sha256(data).hexdigest()
if not 1 <= len(records) <= 32:
    raise SystemExit('Expected 1 to 32 payloads')
table = b''.join(records)
footer = struct.pack('<8sIIII', b'7ZDS31\0\0', len(image), len(records), len(table), zlib.crc32(table))
image.extend(table)
image.extend(footer)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_bytes(image)
a.output.with_suffix('.json').write_text(json.dumps({'installer_sha256': hashlib.sha256(image).hexdigest(),
    'files_sha256': manifest, 'all_payload_files_embedded': True}, indent=2)+'\n')
