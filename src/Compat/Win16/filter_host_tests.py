#!/usr/bin/env python3
"""Byte comparisons and extraction rollback through the native CLI backend."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--console', type=Path, required=True)
p.add_argument('--faults', type=Path, required=True)
p.add_argument('--fixtures', type=Path, required=True)
a = p.parse_args()
console, faults, fixtures = (v.resolve() for v in (a.console, a.faults, a.fixtures))
manifest = json.loads((fixtures / 'manifest.json').read_text())
expected = (fixtures / 'LARGE.BIN').read_bytes()
with tempfile.TemporaryDirectory(prefix='win16-filters-') as temporary:
    root = Path(temporary)
    cache = root / 'CACHE'
    cache.mkdir()
    env = dict(os.environ, TEMP=str(cache), TMP=str(cache))
    for name, case in manifest['archives'].items():
        archive = fixtures / name
        assert hashlib.sha256(archive.read_bytes()).hexdigest() == case['sha256']
        output = root / archive.stem
        for command in (['t', archive], ['x', archive, output]):
            result = subprocess.run([str(console), *map(str, command)], cwd=root,
                                    env=env, capture_output=True, text=True)
            assert result.returncode == 0 and not result.stderr, (command, result.stdout, result.stderr)
            assert not list(cache.iterdir())
        assert (output / 'LARGE.BIN').read_bytes() == expected, name
        print('PASS', name, len(expected), 'exact bytes and dictionary-file cleanup', flush=True)
    for name in ('FBCJ.7Z', 'FCHAIN3.7Z', 'FCOPY.7Z', 'FDFLT.7Z'):
        result = subprocess.run([str(faults), str(fixtures / name)], cwd=root,
                                env=env, capture_output=True, text=True)
        assert result.returncode == 0 and not result.stderr, (name, result.stdout, result.stderr)
        assert not list(cache.iterdir())
        print('PASS', name, 'output-write/close/cancellation rollback', flush=True)
for name, case in manifest['archives'].items():
    assert hashlib.sha256((fixtures / name).read_bytes()).hexdigest() == case['sha256']
print('PASS all filter source archives unchanged')
