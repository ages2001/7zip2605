"""Exercise exact, read-only and unrepresentable extraction metadata."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--console', required=True, type=Path)
p.add_argument('--fixtures', required=True, type=Path)
a = p.parse_args()
console = a.console.resolve()
fixtures = a.fixtures.resolve()
fixtures.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    def run(*args):
        r = subprocess.run([str(console), *args], cwd=root, capture_output=True, text=True)
        assert r.returncode == 0, (r.stdout, r.stderr)
        assert not r.stderr, r.stderr
        return r.stdout
    for name in ('RONLY.TXT', 'MFILE.TXT'):
        (root/name).write_bytes((name+'\r\n').encode()*100)
        os.utime(root/name, (946684800, 946684800))
    (root/'RONLY.TXT').chmod(0o444)
    run('a', str(fixtures/'META32.7Z'), 'RONLY.TXT', 'MFILE.TXT')
    before = (fixtures/'META32.7Z').read_bytes()
    assert 'WARNING' not in run('x', str(fixtures/'META32.7Z'), 'OUT')
    for name in ('RONLY.TXT', 'MFILE.TXT'):
        out = root/'OUT'/name
        assert out.read_bytes() == (root/name).read_bytes()
        assert out.stat().st_mtime == 946684800
    assert not (root/'OUT/RONLY.TXT').stat().st_mode & 0o222
    assert (root/'OUT/MFILE.TXT').stat().st_mode & 0o200
    assert before == (fixtures/'META32.7Z').read_bytes()
    print('PASS exact modification times, read-only permissions, unchanged archive')
    os.utime(root/'MFILE.TXT', (1, 1))
    run('a', 'OLD.7Z', 'MFILE.TXT')
    assert 'modification times' in run('x', 'OLD.7Z', 'OLDOUT')
    os.utime(root/'MFILE.TXT', (946684801, 946684801))
    run('a', 'ODD.7Z', 'MFILE.TXT')
    assert 'precision' in run('x', 'ODD.7Z', 'ODDOUT')
    print('PASS dates before DOS epoch and odd-second precision are reported')
