#!/usr/bin/env python3
"""Decode native raw Deflate with independent zlib and exercise I/O faults."""
import argparse,pathlib,tempfile,subprocess,zlib,random
p=argparse.ArgumentParser();p.add_argument('encoder');a=p.parse_args();rng=random.Random(3916)
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp);source=root/'SOURCE.BIN';output=root/'OUT.DEF'
 cases=[b'',b'A',b'AB',b'ABC',bytes(range(256))*600,b'A'*300000,b'abcdef'*50000,rng.randbytes(150000)]
 for n in [2,3,7,257,258,259,8191,8192,8193,12288,16383,32767,32768,65535,65536]:
  cases.extend([rng.randbytes(n),b'A'*n,(b'abcdefg'*(n//7+1))[:n]])
 for i in range(100):
  pattern=rng.randbytes(rng.randrange(1,1024));cases.append((pattern*rng.randrange(1,300))+rng.randbytes(rng.randrange(512)))
 for i,data in enumerate(cases):
  source.write_bytes(data)
  r=subprocess.run([a.encoder,str(source),str(output),'0','0','13'],capture_output=True,text=True)
  assert r.returncode==0 and 'outstanding=0' in r.stdout and not r.stderr,(i,r.stdout,r.stderr)
  packed=output.read_bytes();assert zlib.decompress(packed,-15)==data,i
  decoder=zlib.decompressobj(-15);assert decoder.decompress(packed)==data and decoder.eof and not decoder.unused_data
  if i in [4,5,6]:assert len(packed)<len(data)//5,(i,len(packed))
 source.write_bytes(bytes(range(256))*600)
 for alloc,io in [(n,0) for n in range(1,6)]+[(0,1),(0,2)]:
  r=subprocess.run([a.encoder,str(source),str(output),str(alloc),str(io)],capture_output=True,text=True)
  assert r.returncode==2 and 'outstanding=0' in r.stdout and not r.stderr and not output.exists(),(alloc,io,r.stdout,r.stderr)
 print(f'PASS {len(cases)} independently decoded Deflate streams, 13-byte reads, compression, 5 allocation faults and read/write failures')
