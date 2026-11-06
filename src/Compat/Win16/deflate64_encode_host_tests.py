#!/usr/bin/env python3
"""Check native Deflate64 output with independent 7-Zip and injected failures."""
import argparse,pathlib,tempfile,subprocess,zlib,random,struct,re
p=argparse.ArgumentParser();p.add_argument('--encoder',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args();rng=random.Random(84)
with tempfile.TemporaryDirectory()as tmp:
 root=pathlib.Path(tmp);src=root/'IN.BIN';out=root/'OUT.DEF';archive=root/'OUT.ZIP'
 cases=[b'',b'A',b'AB',b'ABC',bytes(range(256))*600,b'A'*300000,b'abcdef'*50000,rng.randbytes(150000)]
 for n in [2,3,7,257,258,259,8191,8192,8193,12288,16383,32767,32768,65535,65536]:
  cases.extend([rng.randbytes(n),b'A'*n,(b'abcdefg'*(n//7+1))[:n]])
 for _ in range(100):
  pattern=rng.randbytes(rng.randrange(1,1024));cases.append(pattern*rng.randrange(1,300)+rng.randbytes(rng.randrange(512)))
 def run(alloc=0,io=0,good=True):
  r=subprocess.run([a.encoder,str(src),str(out),str(alloc),str(io),'13','1'],capture_output=True,text=True)
  assert not r.stderr and 'outstanding=0'in r.stdout and (r.returncode==0)==good,(r.stdout,r.stderr)
  if not good:assert not out.exists()
  return int(re.search(r'allocations=(\d+)',r.stdout)[1])
 for index,data in enumerate(cases):
  src.write_bytes(data);count=run();raw=out.read_bytes();name=b'DATA.BIN';crc=zlib.crc32(data)
  local=struct.pack('<I5H3I2H',0x04034b50,21,0,9,0,33,crc,len(raw),len(data),len(name),0)+name+raw
  central=struct.pack('<I6H3I5H2I',0x02014b50,20,21,0,9,0,33,crc,len(raw),len(data),len(name),0,0,0,0,0,0)+name
  archive.write_bytes(local+central+struct.pack('<I4H2IH',0x06054b50,0,0,1,1,len(central),len(local),0))
  r=subprocess.run([a.sevenzip,'x','-so',str(archive)],capture_output=True);assert r.returncode==0 and r.stdout==data,(index,r.stderr)
  if index in [4,5,6]:assert len(raw)<len(data)//5
 src.write_bytes(bytes(range(256))*600);count=run()
 for n in range(1,count+1):run(alloc=n,good=False)
 for n in (1,2):run(io=n,good=False)
 print('PASS',len(cases),'independently decoded Deflate64 streams, 13-byte reads, compression, all',count,'allocation faults and read/write failures')
