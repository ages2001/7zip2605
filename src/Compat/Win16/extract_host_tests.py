"""Check actual extraction results and reject malformed DOS paths."""
import argparse
import hashlib
import pathlib
import subprocess
import tempfile
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--console', type=pathlib.Path, required=True)
p.add_argument('--faults', type=pathlib.Path, required=True)
p.add_argument('--fixtures', type=pathlib.Path, required=True)
a = p.parse_args()
fixtures = a.fixtures.resolve()
console = a.console.resolve()
faults = a.faults.resolve()
originals = {path: hashlib.sha256(path.read_bytes()).digest() for path in fixtures.glob('*.7Z')}
with tempfile.TemporaryDirectory(prefix='win16-extraction-') as temporary:
    root = pathlib.Path(temporary)
    def extract(name, target, expected):
        result = subprocess.run([str(console), 'x', str(fixtures / name), str(target)], capture_output=True, text=True)
        assert result.returncode == expected, (name, result.returncode, result.stdout, result.stderr)
        assert not result.stderr, result.stderr
    output = root / 'OUT'
    extract('EXTRACT.7Z', output, 0)
    for path in (fixtures / 'inputs').rglob('*'):
        actual = output / path.relative_to(fixtures / 'inputs')
        if path.is_dir():
            assert actual.is_dir(), actual
        else:
            assert actual.read_bytes() == path.read_bytes(), actual
    print('PASS solid extraction, nested and empty directories, exact file bytes')
    (output / 'KEEP.TXT').write_bytes(b'Existing destination must not change')
    before = {p.relative_to(output): p.read_bytes() for p in output.rglob('*') if p.is_file()}
    extract('EXTRACT.7Z', output, 2)
    assert before == {p.relative_to(output): p.read_bytes() for p in output.rglob('*') if p.is_file()}
    print('PASS existing destination and user data preserved')
    for name in ('TRAV', 'DEVICE', 'DRIVE', 'LONG', 'UNICODE', 'COLLIDE', 'PARENT'):
        destination = root / name
        extract(name + '.7Z', destination, 2)
        assert not destination.exists(), name
        print('PASS rejected before output:', name)
    assert not (root / 'ESCAPE.TXT').exists()
    subprocess.run([str(faults), str(fixtures / 'EXTRACT.7Z')], cwd=root, check=True)
for path, digest in originals.items():
    assert hashlib.sha256(path.read_bytes()).digest() == digest, path
print('PASS all source archives unchanged')
