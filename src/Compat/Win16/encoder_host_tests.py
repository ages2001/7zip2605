"""Validate segmented LZMA encoding using Python's independent liblzma decoder."""
import argparse
from pathlib import Path
import lzma
import random
import re
import subprocess
import tempfile
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--encoder', type=Path, required=True)
a = p.parse_args()
encoder = a.encoder.resolve()
rng = random.Random(3416)
with tempfile.TemporaryDirectory(prefix='win16-encoder-') as tmp:
    root = Path(tmp)
    cases = [b'', b'A', b'AB', bytes(range(256)), b'A'*65537,
             b'Native Win16 LZMA compression\r\n'*9000,
             rng.randbytes(140000), (bytes(range(256))*513)+rng.randbytes(10000)]
    cases += [rng.randbytes(n) for n in (2, 3, 7, 272, 273, 274, 8191, 8192, 8193, 16383)]
    for index, data in enumerate(cases):
        source = root/'INPUT.BIN';source.write_bytes(data)
        for mode in (0, 1):
            output = root/f'{index}-{mode}.lzma'
            result = subprocess.run([str(encoder), str(source), str(output), '0', '13', str(mode)], capture_output=True, text=True)
            assert result.returncode == 0 and 'outstanding=0' in result.stdout, (index, mode, result.stdout, result.stderr)
            assert not result.stderr, result.stderr
            assert lzma.decompress(output.read_bytes(), format=lzma.FORMAT_ALONE) == data, (index, mode)
        assert source.read_bytes() == data
    print('PASS independent decompression: empty, short, random, repetitive, >64 KiB, both parsing modes, 13-byte input reads')
    source.write_bytes(bytes(range(256))*400)
    for mode in (0, 1):
        output = root/f'COUNT{mode}.LZM'
        result = subprocess.run([str(encoder), str(source), str(output), '0', '13', str(mode)], capture_output=True, text=True)
        assert result.returncode == 0 and 'outstanding=0' in result.stdout and not result.stderr, result
        assert lzma.decompress(output.read_bytes(), format=lzma.FORMAT_ALONE) == source.read_bytes()
        total = int(re.search(r'allocations=(\d+)', result.stdout).group(1))
        for fail in range(1, total + 1):
            output = root/f'FAIL{mode}-{fail}.LZM'
            result = subprocess.run([str(encoder), str(source), str(output), str(fail), '13', str(mode)], capture_output=True, text=True)
            assert result.returncode == 2 and 'outstanding=0' in result.stdout, (mode, fail, result.stdout, result.stderr)
            assert not result.stderr and not output.exists(), (mode, fail, result.stderr)
        print(f'PASS all {total} allocation failures in parsing mode {mode}: no leaked blocks or incomplete output')
