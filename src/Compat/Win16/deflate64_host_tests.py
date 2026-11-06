#!/usr/bin/env python3
"""Verify extended lengths/distances against an independent 7-Zip decoder."""
import argparse, os, random, re, struct, subprocess, tempfile, zlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--probe',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args();cases=0
bases=[1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,32769,49153]
def zipraw(raw,expected):
 name=b'DATA.BIN';crc=zlib.crc32(expected)
 local=struct.pack('<I5H3I2H',0x04034b50,21,0,9,0,0,crc,len(raw),len(expected),len(name),0)+name+raw
 central=struct.pack('<I6H3I5H2I',0x02014b50,20,21,0,9,0,0,crc,len(raw),len(expected),len(name),0,0,0,0,0,0)+name
 return local+central+struct.pack('<I4H2IH',0x06054b50,0,0,1,1,len(central),len(local),0)
def match(history,distance,length):
 bits=[]
 def put(v,n):bits.extend((v>>i)&1 for i in range(n))
 def code(v,n):bits.extend((v>>i)&1 for i in range(n-1,-1,-1))
 put(3,3);code(0xc5,8);put(length-3,16)
 symbol=max(i for i,v in enumerate(bases)if v<=distance);code(symbol,5);put(distance-bases[symbol],0 if symbol<4 else symbol//2-1);code(0,7)
 raw=b''
 for offset in range(0,len(history),32768):
  chunk=history[offset:offset+32768];raw+=struct.pack('<BHH',0,len(chunk),len(chunk)^65535)+chunk
 raw+=bytes(sum(bit<<j for j,bit in enumerate(bits[i:i+8]))for i in range(0,len(bits),8))
 expected=bytearray(history)
 for _ in range(length):expected.append(expected[-distance])
 return raw,bytes(expected)
with tempfile.TemporaryDirectory()as tmp:
 root=Path(tmp);rng=random.Random(83);history=rng.randbytes(65536)
 def run(raw,expected,paged,fail=0,writefail=0,good=True,declared=None):
  global cases
  (root/'IN.DEF').write_bytes(raw);out=root/'OUT.BIN'
  command=[a.probe,str(root/'IN.DEF'),str(out),str(len(expected)if declared is None else declared),'13',str(fail),str(writefail),'4096',str(paged),'1']
  r=subprocess.run(command,capture_output=True,text=True,cwd=root,env=dict(os.environ,TEMP=str(root)))
  assert not r.stderr and 'outstanding=0'in r.stdout,(r.stdout,r.stderr)
  assert (r.returncode==0)==good,(command,r.stdout)
  assert not list(root.glob('*.DIC'))
  if good:assert out.read_bytes()==expected and int(re.search(r'crc=([0-9a-f]+)',r.stdout)[1],16)==zlib.crc32(expected)
  else:assert not out.exists()
  cases+=1;return int(re.search(r'allocations=(\d+)',r.stdout)[1])
 def independent(raw,expected):
  (root/'IN.ZIP').write_bytes(zipraw(raw,expected));r=subprocess.run([a.sevenzip,'x','-so',str(root/'IN.ZIP')],capture_output=True)
  assert r.returncode==0 and r.stdout==expected,(r.returncode,r.stderr)
 for distance in [1,2,4096,16384,32768,32769,49152,49153,65536]:
  for length in [3,257,258,259,65535,65536,65538]:
   raw,expected=match(history,distance,length);independent(raw,expected)
   for paged in [0,1]:run(raw,expected,paged)
 print('PASS 63 independently decoded extended length/distance combinations in RAM and paged modes')
 for paged in [0,1]:
  raw,expected=match(history,65536,65538);count=run(raw,expected,paged)
  for n in range(1,count+1):run(raw,expected,paged,fail=n,good=False)
  run(raw,expected,paged,writefail=1,good=False)
  for changed in [raw[:-1],raw+b'\0']:run(changed,expected,paged,good=False)
  for declared in [len(expected)-1,len(expected)+1]:run(raw,expected,paged,good=False,declared=declared)
 # Independent producer exercises dynamic trees and ordinary short matches.
 for index,data in enumerate([b'abc'*90000,bytes(range(256))*1200,(rng.randbytes(64000)*3),b'A'*300000]):
  folder=root/str(index);folder.mkdir();(folder/'DATA.BIN').write_bytes(data)
  subprocess.run([a.sevenzip,'a','-tzip','-mm=Deflate64','-mx=9','IN.ZIP','DATA.BIN'],cwd=folder,check=True,capture_output=True)
  packed=(folder/'IN.ZIP').read_bytes();method=struct.unpack_from('<H',packed,8)[0];assert method==9,(index,method)
  size=struct.unpack_from('<I',packed,18)[0];name,extra=struct.unpack_from('<HH',packed,26);raw=packed[30+name+extra:30+name+extra+size]
  for paged in [0,1]:run(raw,data,paged)
 print('PASS',cases,'Deflate64 cases; exact bytes/CRC, malformed sizes, allocation/output failures and cache cleanup')
