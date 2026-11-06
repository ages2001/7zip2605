"""Generate Unicode simple uppercase pairs from UnicodeData.txt field 12."""
import argparse
import hashlib
from pathlib import Path
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('input', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
data = a.input.read_bytes()
pairs = []
for line in data.decode('utf-8').splitlines():
    fields = line.split(';')
    if fields[12]:
        pairs.append((int(fields[0], 16), int(fields[12], 16)))
assert pairs == sorted(pairs)
a.output.write_text('/* Unicode 16.0 simple uppercase mappings (ICU release-76-1).\n'
    + '   UnicodeData.txt SHA-256: ' + hashlib.sha256(data).hexdigest() + ' */\n'
    + 'static const unsigned long dos_upper[][2] = {\n'
    + ''.join('  {0x%X, 0x%X},\n' % pair for pair in pairs) + '};\n')
