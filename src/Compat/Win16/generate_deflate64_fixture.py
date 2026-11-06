#!/usr/bin/env python3
"""Make maximum-distance/length Deflate64 data and verify it independently."""
import argparse, random, struct, subprocess, zlib
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--sevenzip', required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
history=random.Random(83).randbytes(65536);bits=[]
def put(value,count):bits.extend((value>>i)&1 for i in range(count))
def code(value,count):bits.extend((value>>i)&1 for i in range(count-1,-1,-1))
put(3,3)
for _ in range(3):
    code(0xc5,8)  # Fixed symbol 285 has 16 length bits in Deflate64.
    put(65535,16)  # Length 3 + 65535 = 65538.
    code(31,5)
    put(16383,14)  # Distance 49153 + 16383 = 65536.
code(0,7)
raw=b''.join(struct.pack('<BHH',0,32768,32767)+history[i:i+32768] for i in (0,32768))
raw+=bytes(sum(bit<<j for j,bit in enumerate(bits[i:i+8])) for i in range(0,len(bits),8))
expected=(history*5)[:65536+3*65538];crc=zlib.crc32(expected);name=b'DATA.BIN'
local=struct.pack('<I5H3I2H',0x04034b50,21,0,9,0,33,crc,len(raw),len(expected),len(name),0)+name+raw
central=struct.pack('<I6H3I5H2I',0x02014b50,20,21,0,9,0,33,crc,len(raw),len(expected),len(name),0,0,0,0,0,0)+name
archive=local+central+struct.pack('<I4H2IH',0x06054b50,0,0,1,1,len(central),len(local),0)
(a.output/'MAX64.ZIP').write_bytes(archive)
r=subprocess.run([a.sevenzip,'x','-so',str(a.output/'MAX64.ZIP')],capture_output=True)
assert r.returncode==0 and r.stdout==expected,(r.returncode,r.stderr)
(a.output/'MAX64.DEF').write_bytes(raw);(a.output/'MAX64.BIN').write_bytes(expected)
print('PASS independent 7-Zip verification of 65536 distance / 65538 length fixture')
