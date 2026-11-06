"""Create input archives for the native streaming, console and GUI fixtures.

The supplied 7-Zip executable is a host-side fixture generator. It is not
included in, or evidence for, the target application's compatibility.
"""
import argparse
import pathlib
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sevenzip', required=True, type=pathlib.Path)
parser.add_argument('--output', required=True, type=pathlib.Path)
args = parser.parse_args()
sevenzip = args.sevenzip.resolve()
out = args.output.resolve()
if out.exists() and any(out.iterdir()):
    raise SystemExit(f"Fixture output directory must be empty: {out}")
out.mkdir(parents=True, exist_ok=True)
pattern = b'Streaming 7-Zip across 16-bit segment boundaries.\r\n'
(out / 'DATA.TXT').write_bytes((pattern * 6000)[:262267])


def archive(name, method, inputs, cwd):
    target = out / name
    if target.exists():
        raise SystemExit(f'Refusing to update existing test archive: {target}')
    command = [str(sevenzip), 'a', '-t7z', '-m0=' + method,
               '-mhc=off', '-mmt=1']
    if method != 'Copy':
        command.append('-md=32k')
    subprocess.run(command + [str(target)] + inputs, cwd=cwd, check=True)


for tag, method in [('COPY', 'Copy'), ('LZMA', 'LZMA'), ('LZM2', 'LZMA2')]:
    archive('S16' + tag + '.7Z', method, ['DATA.TXT'], out)

inputs = out / 'multi-input'
(inputs / 'EMPTY').mkdir(parents=True, exist_ok=True)
(inputs / 'SUB').mkdir(exist_ok=True)
(inputs / 'A.TXT').write_bytes(b'First file boundary.\n' * 9000)
(inputs / 'SUB' / 'B.TXT').write_bytes(bytes(range(256)) * 333)
(inputs / 'ZERO.TXT').write_bytes(b'')
(inputs / 'UNICODE-\u0131.txt').write_bytes(b'Unicode filename contents\n')
archive('MULTI.7Z', 'LZMA2', ['.'], inputs)
copy = (out / 'S16COPY.7Z').read_bytes()
# The one-file Copy fixture starts its uncompressed data after the 32-byte
# signature/start header. Corrupt data only; retain a readable archive header.
assert copy[32:32 + len(pattern)] == pattern
bad = bytearray(copy)
bad[32] ^= 1
(out / 'COPY.7Z').write_bytes(copy)
(out / 'BADCRC.7Z').write_bytes(bad)
(out / 'TRUNC.7Z').write_bytes(copy[:-8])
