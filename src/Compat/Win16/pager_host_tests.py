#!/usr/bin/env python3
"""Compare paged LZMA/LZMA2 extraction with independent 7-Zip output."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--console', type=Path, required=True)
p.add_argument('--faults', type=Path, required=True)
p.add_argument('--fixtures', type=Path, required=True)
p.add_argument('--sevenzip', type=Path, required=True)
a = p.parse_args()
console, faults, fixtures, sevenzip = [v.resolve() for v in (a.console, a.faults, a.fixtures, a.sevenzip)]
before = {f: hashlib.sha256(f.read_bytes()).digest() for f in fixtures.glob('*.7Z')}
with tempfile.TemporaryDirectory(prefix='win16-paging-') as temporary:
    root = Path(temporary)
    cache = root / 'CACHE'
    cache.mkdir()
    env = dict(os.environ, TEMP=str(cache), TMP=str(cache))
    expected = (fixtures / 'LARGE.BIN').read_bytes()

    def run(args, success=True, environment=env):
        r = subprocess.run([str(console), *map(str, args)], cwd=root, env=environment,
                           capture_output=True, text=True)
        assert (r.returncode == 0) == success, (args, r.stdout, r.stderr)
        assert not r.stderr, r.stderr
        assert not list(cache.iterdir()), list(cache.iterdir())
        return r.stdout

    for name in ('LZ128', 'LZ32M', 'L2128', 'L232M'):
        archive = fixtures / (name + '.7Z')
        run(['t', archive])
        out = root / name
        run(['x', archive, out])
        assert (out / 'LARGE.BIN').read_bytes() == expected
        reference = root / ('REF' + name)
        subprocess.run([str(sevenzip), 'x', '-y', '-o' + str(reference), str(archive)],
                       check=True, stdout=subprocess.DEVNULL)
        assert (reference / 'LARGE.BIN').read_bytes() == expected
        print('PASS', name, '360000 exact bytes, 90000-byte distances and temporary-file cleanup')
    result = run(['x', fixtures / 'L2BAD.7Z', root / 'BAD'], False)
    assert 'code 3' in result and not (root / 'BAD').exists(), result
    result = run(['x', fixtures / 'L232M.7Z', root / 'IOFAIL'], False,
                 dict(env, TEMP=str(root / 'MISSING')))
    assert 'code 9' in result and not (root / 'IOFAIL').exists(), result
    print('PASS CRC and backing-file creation errors remove partial extraction')
    subprocess.run([str(faults), str(fixtures / 'L232M.7Z')], cwd=root, env=env, check=True)
    assert not list(cache.iterdir())
    run(['t', fixtures / 'L232M.7Z'])
    print('PASS output-write/close/cancel rollback and a fresh operation after failure')
assert before == {f: hashlib.sha256(f.read_bytes()).digest() for f in before}
print('PASS all source archives unchanged')
