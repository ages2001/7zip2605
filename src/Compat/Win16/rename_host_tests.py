#!/usr/bin/env python3
"""Independent checks for lossless 7z metadata-only updates."""
import argparse, pathlib, subprocess, tempfile, struct, zlib, os
p=argparse.ArgumentParser();p.add_argument('--console',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp);src=root/'FIRST.TXT';src.write_bytes(bytes(range(256))*600);os.utime(src,(946684800,946684800))
 def run(args,good=True):
  r=subprocess.run([a.console,*args],cwd=root,capture_output=True,text=True)
  assert (r.returncode==0)==good,(r.stdout,r.stderr)
  assert not r.stderr,r.stderr
  return r.stdout
 def header(data):
  offset,size=struct.unpack_from('<QQ',data,12);return data[32+offset:32+offset+size]
 def inject(data):
  offset,size=struct.unpack_from('<QQ',data,12);old=header(data)
  opaque=b'opaque property: \x11\x03\x00X\x00\x00\x00 preserve exactly'
  new=old[:-2]+bytes([64,len(opaque)])+opaque+old[-2:]
  b=bytearray(data[:32+offset]+new);struct.pack_into('<Q',b,20,len(new));struct.pack_into('<I',b,28,zlib.crc32(new));struct.pack_into('<I',b,8,zlib.crc32(b[12:32]));return bytes(b),opaque
 for method in ['copy','lzma']:
  archive=root/(method+'.7Z');run(['a',str(archive),'-m0='+method,str(src)])
  original,opaque=inject(archive.read_bytes());archive.write_bytes(original)
  (root/'7R000000.TMP').write_bytes(b'keep temporary');(root/'7R000000.BAK').write_bytes(b'keep backup')
  run(['rn',str(archive),'FIRST.TXT','NEW.TXT']);updated=archive.read_bytes()
  assert updated[32:len(original)]==original[32:]
  assert opaque in header(updated)
  oldprop=bytes([17,21,0])+'FIRST.TXT\0'.encode('utf-16le')
  newprop=bytes([17,17,0])+'NEW.TXT\0'.encode('utf-16le')
  assert header(updated)==header(original).replace(oldprop,newprop,1)
  out=root/(method+'OUT');r=subprocess.run([a.sevenzip,'x',str(archive),'-o'+str(out)],capture_output=True);assert r.returncode==0,(r.stdout,r.stderr)
  assert (out/'NEW.TXT').read_bytes()==src.read_bytes();assert int((out/'NEW.TXT').stat().st_mtime)==946684800
  assert (root/'7R000000.TMP').read_bytes()==b'keep temporary';assert (root/'7R000000.BAK').read_bytes()==b'keep backup'
  run(['t',str(archive)])
  snapshot=archive.read_bytes()
  for old,new in [('ABSENT','OTHER'),('NEW.TXT','../BAD.TXT'),('NEW.TXT','CON')]:
   run(['rn',str(archive),old,new],False);assert archive.read_bytes()==snapshot
  run(['rn',str(archive),'NEW.TXT','NEW.TXT']);assert archive.read_bytes()==snapshot
 # Encoded headers, solid directories and untouched Unicode names from host 7z.
 folder=root/'DIR';folder.mkdir();(folder/'A.TXT').write_bytes(b'alpha'*20000);(folder/'B.TXT').write_bytes(b'beta'*10000);(root/'KEEP.TXT').write_bytes(b'keep');(root/'snow-\u96ea.txt').write_bytes(b'unicode')
 archive=root/'SOLID.7Z'
 r=subprocess.run([a.sevenzip,'a','-t7z','-m0=LZMA2:d=32k',str(archive),'DIR','KEEP.TXT','snow-\u96ea.txt'],cwd=root,capture_output=True);assert r.returncode==0,(r.stdout,r.stderr)
 original=archive.read_bytes();assert header(original)[0]==23
 run(['rn',str(archive),'DIR','RENAMED']);updated=archive.read_bytes();assert updated[32:len(original)]==original[32:]
 out=root/'SOLIDOUT';r=subprocess.run([a.sevenzip,'x',str(archive),'-o'+str(out)],capture_output=True);assert r.returncode==0,(r.stdout,r.stderr)
 assert (out/'RENAMED/A.TXT').read_bytes()==(folder/'A.TXT').read_bytes();assert (out/'RENAMED/B.TXT').read_bytes()==(folder/'B.TXT').read_bytes();assert (out/'snow-\u96ea.txt').read_bytes()==b'unicode'
 snapshot=archive.read_bytes()
 for old,new in [('KEEP.TXT','RENAMED'),('KEEP.TXT','renamed/a.txt'),('RENAMED','KEEP.TXT')]:
  run(['rn',str(archive),old,new],False);assert archive.read_bytes()==snapshot
 print('PASS lossless Copy/LZMA/solid encoded-header rename, folder subtree, opaque metadata, Unicode preservation, collisions and protected temporary/backup files')
