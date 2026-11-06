#!/usr/bin/env python3
"""Compare all native checksum backends with independent implementations."""
import argparse,hashlib,pathlib,re,subprocess,tempfile,zlib
p=argparse.ArgumentParser();p.add_argument('--probe',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp)
 for size in [0,3,63,64,65,127,128,129,511,512,513,65535,65536,131071]:
  data=b'abc' if size==3 else bytes((i*17^(i>>7))&255 for i in range(size));path=root/'DATA.BIN';path.write_bytes(data)
  r=subprocess.run([a.probe,str(path)],capture_output=True,text=True);assert r.returncode==0 and not r.stderr,(size,r.stdout,r.stderr)
  found=dict(line.split() for line in r.stdout.splitlines());assert len(found)==11
  for method,pyname in [('MD5','md5'),('SHA1','sha1'),('SHA256','sha256'),('SHA384','sha384'),('SHA512','sha512'),('SHA3-256','sha3_256')]:assert found[method]==hashlib.new(pyname,data).hexdigest().upper(),(size,method)
  assert found['CRC32']=='%08X'%(zlib.crc32(data)&0xffffffff)
  r=subprocess.run(['cksum',str(path)],capture_output=True,text=True,check=True);assert found['CKSUM']=='%08X'%int(r.stdout.split()[0])
  r=subprocess.run([a.sevenzip,'h','-scrc*',str(path)],capture_output=True,text=True);assert not r.returncode,(r.stdout,r.stderr)
  other=dict(re.findall(r'^(\S+)\s+for data:\s+([0-9A-Fa-f]+)$',r.stdout,re.M));assert len(other)==10
  for method,digest in other.items():assert found[method]==digest.upper(),(size,method,found[method],digest)
  assert path.read_bytes()==data
 print('PASS all 11 checksums against independent 7-Zip, hashlib, zlib and POSIX cksum; empty/abc, block boundaries and files over 64 KiB; inputs preserved')
