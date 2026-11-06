"""Cross-check native backend creation with an independent 7-Zip executable."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--console', required=True, type=Path)
p.add_argument('--sevenzip', required=True, type=Path)
a = p.parse_args()
console, sevenzip = a.console.resolve(), a.sevenzip.resolve()
with tempfile.TemporaryDirectory(prefix='win16-create-') as tmp:
    root = Path(tmp)
    files = {'FIRST.TXT': b'Native Win16 creation\r\n'*12000,
             'SECOND.BIN': bytes(range(256))*400, 'ZERO.TXT': b'', 'EMPTY.TXT': b''}
    for name, data in files.items():
        (root/name).write_bytes(data)
        os.utime(root/name, (946684800, 946684800))
    def create(name, sources, code=0, method='copy'):
        r = subprocess.run([str(console), 'a', name, '-m0='+method, *sources], cwd=root,
                           capture_output=True, text=True)
        assert r.returncode == code, (r.returncode, r.stdout, r.stderr)
        assert not r.stderr, r.stderr
    create('NEW.7Z', ['FIRST.TXT', 'SECOND.BIN', 'ZERO.TXT'])
    before = (root/'NEW.7Z').read_bytes()
    create('NEW.7Z', ['EMPTY.TXT'], 2)
    assert (root/'NEW.7Z').read_bytes() == before
    create('EMPTY.7Z', ['ZERO.TXT', 'EMPTY.TXT'])
    create('LZMA.7Z', ['FIRST.TXT', 'SECOND.BIN', 'ZERO.TXT'], method='lzma')
    assert (root/'LZMA.7Z').stat().st_size < 10000
    for archive in ['NEW.7Z', 'EMPTY.7Z', 'LZMA.7Z']:
        dest = root/(archive+'.OUT')
        r = subprocess.run([str(sevenzip), 'x', str(root/archive), '-o'+str(dest)], capture_output=True)
        assert r.returncode == 0, (r.stdout, r.stderr)
        for f in dest.iterdir():
            assert f.read_bytes() == files[f.name], f
            assert abs(f.stat().st_mtime-946684800) <= 2, f.stat().st_mtime
        result = subprocess.run([str(console), 't', str(root/archive)], capture_output=True, text=True)
        assert result.returncode == 0 and not result.stderr, (result.stdout, result.stderr)
    (root/'first.txt').write_bytes(b'case collision')
    create('COLLIDE.7Z', ['FIRST.TXT', 'first.txt'], 2)
    assert not (root/'COLLIDE.7Z').exists()
    create('MISSING.7Z', ['ABSENT.TXT'], 2)
    assert not (root/'MISSING.7Z').exists()
    for name, data in files.items():
        assert (root/name).read_bytes() == data
    print('PASS Copy/LZMA multi-file creation, >64 KiB content, empty files, all-empty archive')
    print('PASS independent extraction matches exact bytes and modification timestamps')
    print('PASS existing archive preserved, collisions/missing sources rejected, sources unchanged')
