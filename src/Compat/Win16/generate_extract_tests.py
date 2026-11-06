"""Generate independent extraction fixtures using a host 7-Zip executable."""
import argparse
import pathlib
import shutil
import subprocess
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--sevenzip', type=pathlib.Path, required=True)
p.add_argument('--output', type=pathlib.Path, required=True)
a = p.parse_args()
root = a.output.resolve()
if root.exists() and any(root.iterdir()):
    raise SystemExit('Use a new or empty fixture directory')
root.mkdir(parents=True, exist_ok=True)
source = root / 'inputs'
(source / 'SUB' / 'DEEP').mkdir(parents=True)
(source / 'EMPTY').mkdir()
(source / 'FIRST.TXT').write_bytes(b'First file crosses segment boundaries.\r\n' * 6000)
(source / 'SUB' / 'DEEP' / 'SECOND.BIN').write_bytes(bytes(range(256)) * 400)
(source / 'ZERO.TXT').write_bytes(b'')
sevenzip = str(a.sevenzip.resolve())
subprocess.run([sevenzip, 'a', '-t7z', '-m0=LZMA2', '-md=32k', '-mhc=off',
                '-mmt=1', str(root / 'EXTRACT.7Z'), '.'], cwd=source, check=True)
for tag, original, renamed in [
    ('TRAV', 'FIRST.TXT', '../ESCAPE.TXT'),
    ('DEVICE', 'FIRST.TXT', 'NUL.TXT'),
    ('DRIVE', 'FIRST.TXT', 'C:OUT.TXT'),
    ('LONG', 'FIRST.TXT', 'LONGFILENAME.TXT'),
    ('UNICODE', 'FIRST.TXT', 'NAME-\u0131.TXT'),
    ('COLLIDE', 'SUB/DEEP/SECOND.BIN', 'first.txt'),
    ('PARENT', 'SUB/DEEP/SECOND.BIN', 'FIRST.TXT/CHILD.DAT'),
]:
    target = root / (tag + '.7Z')
    shutil.copyfile(root / 'EXTRACT.7Z', target)
    subprocess.run([sevenzip, 'rn', '-ssc', '-mhc=off', str(target), original, renamed], check=True)
