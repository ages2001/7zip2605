#!/usr/bin/env python3
"""Cross-check the segmented raw decoder with Python's independent zlib."""
import argparse, pathlib, random, subprocess, tempfile, zlib, re, os
p=argparse.ArgumentParser();p.add_argument('probe');p.add_argument('--paged',action='store_true');a=p.parse_args()
rng=random.Random(35)
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp);src=root/'IN.DEF';out=root/'OUT.BIN'
 def run(data, size, good, chunk=13, fail=0, writefail=0, maximum=0):
  src.write_bytes(data)
  r=subprocess.run([a.probe,str(src),str(out),str(size),str(chunk),str(fail),str(writefail),str(maximum),str(int(a.paged))],capture_output=True,text=True,cwd=root,env=dict(os.environ,TEMP=str(root)))
  assert 'outstanding=0' in r.stdout,(r.returncode,r.stdout,r.stderr)
  assert not list(root.glob('*.DIC')),r.stdout
  assert (r.returncode==0)==good,(r.returncode,r.stdout,r.stderr,data[:30])
  if good:
   actual=out.read_bytes();assert int(re.search(r"crc=([0-9a-f]+)",r.stdout)[1],16)==zlib.crc32(actual)
   run.allocations=int(re.search(r"allocations=(\d+)",r.stdout)[1])
  else: assert not out.exists()
  return out.read_bytes() if good else b''
 cases=0
 for data in [b'',b'a',b'a'*300000,bytes(range(256))*1300,rng.randbytes(160000),rng.randbytes(32768)*4]+[rng.randbytes(n) for n in range(100)]:
  for level,strategy in [(0,0),(6,0),(9,zlib.Z_FIXED),(6,zlib.Z_HUFFMAN_ONLY),(6,zlib.Z_RLE)]:
   c=zlib.compressobj(level,zlib.DEFLATED,-15,8,strategy);packed=c.compress(data)+c.flush()
   assert run(packed,len(data),True)==data;cases+=1
   if len(packed)>1:run(packed[:-1],len(data),False)
   run(packed+b'\0',len(data),False)
   run(packed,len(data)+1,False)
   if data:run(packed,len(data)-1,False)
 # Multiple blocks, history references across flush and 32 KiB boundaries.
 data=rng.randbytes(32768);c=zlib.compressobj(6,zlib.DEFLATED,-15)
 packed=c.compress(data)+c.flush(zlib.Z_SYNC_FLUSH)+c.compress(data)+c.flush()
 assert run(packed,len(data)*2,True,1)==data*2
 assert run(packed,len(data)*2,True,13,maximum=4096)==data*2
 for fail in range(1,run.allocations+1):run(packed,len(data)*2,False,13,fail)
 run(packed,len(data)*2,False,13,0,1)
 # Force the maximum legal distance, including page and full-window wraps.
 # zlib's compressor normally avoids distance 32768, so write a fixed block.
 history=rng.randbytes(32768);bits=[]
 def put(value,count):bits.extend((value>>i)&1 for i in range(count))
 def code(value,count):bits.extend((value>>i)&1 for i in range(count-1,-1,-1))
 put(3,3) # Final fixed-Huffman block after a nonfinal stored history block.
 for _ in range(128):
  code(0xc5,8);code(29,5);put(8191,13) # Length 258, distance 32768.
 code(0,7)
 tail=bytes(sum(bit<<j for j,bit in enumerate(bits[i:i+8])) for i in range(0,len(bits),8))
 packed=b'\x00\x00\x80\xff\x7f'+history+tail
 expected=(history*3)[:32768+128*258]
 assert zlib.decompress(packed,-15)==expected
 assert run(packed,len(expected),True,1,maximum=4096)==expected
 print('PASS full 32768-byte distances and page/window wraps with 4 KiB maximum allocation')
 # Reserved block type, invalid stored complement, distance before any output.
 for packed in [b'\x07',b'\x01\x01\x00\x00\x00A',b'\x03\x02\x00']:
  run(packed,3,False)
 # Bit mutations are either rejected or must exactly match the reference.
 data=(b'abcdef'*1000)+rng.randbytes(200)
 c=zlib.compressobj(6,zlib.DEFLATED,-15);packed=c.compress(data)+c.flush()
 for _ in range(400):
  bad=bytearray(packed);i=rng.randrange(len(bad));bad[i]^=1<<rng.randrange(8)
  try:
   ref=zlib.decompressobj(-15);decoded=ref.decompress(bad)+ref.flush()
   good=ref.eof and not ref.unused_data and len(decoded)==len(data)
  except zlib.error:good=False
  actual=run(bad,len(data),good)
  if good:assert actual==decoded
 print(f'PASS {cases} stored/fixed/dynamic cases, short reads, boundaries, mutations, rollback and allocation/write failures; paged={a.paged}')
