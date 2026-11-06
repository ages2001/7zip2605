#!/usr/bin/env python3
"""Independent content/metadata and failure checks for native 7z updates."""
import argparse, pathlib, subprocess, tempfile, struct, zlib, os
p=argparse.ArgumentParser();p.add_argument('--console',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp)
 def run(args,good=True):
  r=subprocess.run([a.console,*args],cwd=root,capture_output=True,text=True)
  assert (r.returncode==0)==good,(args,r.stdout,r.stderr)
  assert not r.stderr,r.stderr
  return r.stdout
 def host(*args):
  r=subprocess.run([a.sevenzip,*args],cwd=root,capture_output=True)
  assert r.returncode==0,(args,r.stdout,r.stderr)
  return r.stdout
 def extract(path,expected,label):
  host('t',str(path));out=root/label;host('x',str(path),'-o'+str(out))
  found={str(f.relative_to(out)):f.read_bytes() for f in out.rglob('*') if f.is_file()}
  assert found==expected,(label,found.keys(),expected.keys())
  return out
 def inject(path,tag,value):
  b=bytearray(path.read_bytes());offset,size=struct.unpack_from('<QQ',b,12)
  start=32+offset;header=b[start:start+size]
  assert header[0]==1 and len(value)<128
  header=header[:-2]+bytes([tag,len(value)])+value+header[-2:]
  b=b[:start]+header;struct.pack_into('<Q',b,20,len(header));struct.pack_into('<I',b,28,zlib.crc32(header));struct.pack_into('<I',b,8,zlib.crc32(b[12:32]));path.write_bytes(b)
 first=bytes(range(256))*600;replacement=b'Replacement data\r\n'*5000;addition=b'Added data\r\n'*7000
 for method in ('copy','lzma'):
  src=root/'FIRST.TXT';src.write_bytes(first);os.utime(src,(946684800,946684800))
  keep=root/'KEEP.TXT';keep.write_bytes(b'keep bytes');os.utime(keep,(946684800,946684800))
  zero=root/'ZERO.TXT';zero.write_bytes(b'')
  archive=root/(method+'.7Z');run(['a',str(archive),'-m0='+method,str(src),str(keep),str(zero)])
  # Preserve creation times even when replacing a file's data.
  ctime=struct.pack('<Q',125911584000000000)
  inject(archive,18,b'\x01\x00'+ctime*3)
  for ext in ('TMP','DAT','BAK'):(root/('7R000000.'+ext)).write_bytes(b'not owned')
  src.write_bytes(replacement);os.utime(src,(978307200,978307200))
  added=root/'ADDED.TXT';added.write_bytes(addition)
  run(['a',str(archive),'-m0='+method,str(src),str(added)])
  out=extract(archive,{'FIRST.TXT':replacement,'KEEP.TXT':b'keep bytes','ZERO.TXT':b'','ADDED.TXT':addition},method+'ADD')
  assert int((out/'KEEP.TXT').stat().st_mtime)==946684800
  assert int((out/'FIRST.TXT').stat().st_mtime)==978307200
  listed=host('l','-slt',str(archive)).decode();assert listed.count('Created = 2000-01-01')==3,listed
  # Non-empty <-> empty replacements, with old streams still checked.
  src.write_bytes(b'');zero.write_bytes(b'was empty')
  run(['a',str(archive),str(src),str(zero)])
  extract(archive,{'FIRST.TXT':b'','KEEP.TXT':b'keep bytes','ZERO.TXT':b'was empty','ADDED.TXT':addition},method+'EMPTY')
  snapshot=archive.read_bytes()
  run(['d',str(archive),'ABSENT']);assert archive.read_bytes()==snapshot
  for args in (['a',str(archive),str(added),str(added)],['a',str(archive),'MISSING.TXT'],['d',str(archive),'../KEEP.TXT']):
   run(args,False);assert archive.read_bytes()==snapshot
  run(['d',str(archive),'ADDED.TXT','FIRST.TXT'])
  extract(archive,{'KEEP.TXT':b'keep bytes','ZERO.TXT':b'was empty'},method+'DEL')
  run(['d',str(archive),'KEEP.TXT','ZERO.TXT']);extract(archive,{},method+'NONE')
  run(['a',str(archive),str(added)]);extract(archive,{'ADDED.TXT':addition},method+'AGAIN')
  for ext in ('TMP','DAT','BAK'):assert (root/('7R000000.'+ext)).read_bytes()==b'not owned'
  assert sorted(f.name for f in root.glob('7R*'))==['7R000000.BAK','7R000000.DAT','7R000000.TMP']
 # Solid compressed and encoded headers, directories and untouched Unicode names.
 folder=root/'DIR';folder.mkdir();(folder/'A.TXT').write_bytes(first);(folder/'B.TXT').write_bytes(b'beta'*10000)
 (root/'EMPTY').mkdir();(root/'snow-\u96ea.txt').write_bytes(b'unicode');(root/'KEEP.TXT').write_bytes(b'keep')
 archive=root/'SOLID.7Z';host('a','-t7z','-m0=LZMA2:d=32k',str(archive),'DIR','EMPTY','KEEP.TXT','snow-\u96ea.txt')
 run(['a',str(archive),'-m0=lzma',str(root/'ADDED.TXT')])
 out=extract(archive,{'DIR/A.TXT':first,'DIR/B.TXT':b'beta'*10000,'KEEP.TXT':b'keep','snow-\u96ea.txt':b'unicode','ADDED.TXT':addition},'SOLIDADD');assert (out/'EMPTY').is_dir()
 run(['d',str(archive),'DIR']);out=extract(archive,{'KEEP.TXT':b'keep','snow-\u96ea.txt':b'unicode','ADDED.TXT':addition},'SOLIDDEL');assert (out/'EMPTY').is_dir()
 snapshot=archive.read_bytes();(root/'EMPTY.TXT').write_bytes(b'')
 # Opaque per-file properties must not be silently discarded or misindexed.
 inject(archive,64,b'unknown indexed metadata');snapshot=archive.read_bytes()
 assert 'metadata' in run(['a',str(archive),str(root/'EMPTY.TXT')],False);assert archive.read_bytes()==snapshot
 run(['d',str(archive),'KEEP.TXT'],False);assert archive.read_bytes()==snapshot
 # Verify old CRCs even when deleting/replacing every affected stream.
 corrupt=root/'BAD.7Z';run(['a',str(corrupt),str(root/'ADDED.TXT')]);b=bytearray(corrupt.read_bytes());b[40]^=1;corrupt.write_bytes(b);snapshot=bytes(b)
 for args in (['d',str(corrupt),'ADDED.TXT'],['a',str(corrupt),str(root/'ADDED.TXT')]):
  run(args,False);assert corrupt.read_bytes()==snapshot
 print('PASS add/replace/delete, Copy/LZMA, solid streams, encoded headers, empty files/folders/archive, Unicode, exact content, CTime/MTime, collisions, protected temporary files, unsupported metadata and damaged originals')
