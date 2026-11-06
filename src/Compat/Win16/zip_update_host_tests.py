#!/usr/bin/env python3
"""Independent preservation and transaction checks for native ZIP updates."""
import argparse, io, os, pathlib, random, struct, subprocess, tempfile, zipfile
p=argparse.ArgumentParser();p.add_argument('--console',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp)
 def run(*args,good=True):
  r=subprocess.run([a.console,*map(str,args)],cwd=root,capture_output=True,text=True)
  assert (r.returncode==0)==good,(args,r.stdout,r.stderr)
  assert not r.stderr,r.stderr
  return r.stdout
 def check(path,expected):
  with zipfile.ZipFile(path) as z:
   assert z.testzip() is None
   assert {n:z.read(n) for n in z.namelist()}==expected
  r=subprocess.run([a.sevenzip,'t',str(path)],cwd=root,capture_output=True);assert r.returncode==0,r.stdout
 def raw(path):
  data=path.read_bytes();e=next(i for i in range(len(data)-22,max(-1,len(data)-65558),-1) if data[i:i+4]==b'PK\x05\x06' and i+22+struct.unpack_from('<H',data,i+20)[0]==len(data));c=struct.unpack_from('<I',data,e+16)[0];records={}
  with zipfile.ZipFile(path) as z:
   for info in z.infolist():
    n,x,comment=struct.unpack_from('<HHH',data,c+28);central=data[c:c+46+n+x+comment];local=info.header_offset
    ln,lx=struct.unpack_from('<HH',data,local+26);end=local+30+ln+lx+info.compress_size
    if info.flag_bits&8:end+=16 if data[end:end+4]==b'PK\x07\x08' else 12
    records[info.filename]=(data[local:end],central[:42]+b'\0'*4+central[46:])
    c+=len(central)
   return records,z.comment
 first=random.Random(4101).randbytes(140000);replacement=b'replacement\r\n'*9000;addition=bytes(range(256))*450
 for method in ('copy','deflate'):
  (root/'FIRST.TXT').write_bytes(first);os.utime(root/'FIRST.TXT',(946684801,946684801))
  (root/'KEEP.TXT').write_bytes(b'unchanged');(root/'ZERO.TXT').write_bytes(b'')
  archive=root/(method+'.ZIP');run('a',archive,'-m0='+method,'FIRST.TXT','KEEP.TXT','ZERO.TXT')
  before,_=raw(archive)
  for ext in ('TMP','ZIP','BAK'):(root/('7R000000.'+ext)).write_bytes(b'unowned')
  (root/'FIRST.TXT').write_bytes(replacement);os.utime(root/'FIRST.TXT',(978307201,978307201))
  (root/'ADDED.TXT').write_bytes(addition)
  run('a',archive,'-m0='+method,'FIRST.TXT','ADDED.TXT')
  check(archive,{'FIRST.TXT':replacement,'KEEP.TXT':b'unchanged','ZERO.TXT':b'','ADDED.TXT':addition})
  after,_=raw(archive);assert before['KEEP.TXT']==after['KEEP.TXT'] and before['ZERO.TXT']==after['ZERO.TXT']
  with zipfile.ZipFile(archive) as z:assert struct.unpack('<HHBI',z.getinfo('FIRST.TXT').extra)==(0x5455,5,1,978307201)
  snapshot=archive.read_bytes();run('d',archive,'ABSENT');assert archive.read_bytes()==snapshot
  for args in [('a',archive,'ADDED.TXT','ADDED.TXT'),('a',archive,'MISSING.TXT'),('d',archive,'../KEEP.TXT')]:
   run(*args,good=False);assert archive.read_bytes()==snapshot
  (root/'FIRST.TXT').write_bytes(b'');(root/'ZERO.TXT').write_bytes(b'not empty')
  run('a',archive,'FIRST.TXT','ZERO.TXT');check(archive,{'FIRST.TXT':b'','KEEP.TXT':b'unchanged','ZERO.TXT':b'not empty','ADDED.TXT':addition})
  run('d',archive,'FIRST.TXT','ADDED.TXT');check(archive,{'KEEP.TXT':b'unchanged','ZERO.TXT':b'not empty'})
  run('d',archive,'KEEP.TXT','ZERO.TXT');check(archive,{})
  run('a',archive,'ADDED.TXT');check(archive,{'ADDED.TXT':addition})
  for ext in ('TMP','ZIP','BAK'):assert (root/('7R000000.'+ext)).read_bytes()==b'unowned'
  assert sorted(f.name for f in root.glob('7R*'))==['7R000000.BAK','7R000000.TMP','7R000000.ZIP']
 # Descriptors, opaque metadata, Unicode and comments survive unrelated updates.
 class Unseekable(io.BytesIO):
  def seekable(self):return False
  def seek(self,*args):raise OSError('stream')
 stream=Unseekable();expected={'DIR/':b'','DIR/A.TXT':first,'snow-\u96ea.txt':b'unicode','KEEP.TXT':b'opaque'}
 with zipfile.ZipFile(stream,'w',compression=zipfile.ZIP_DEFLATED) as z:
  z.comment=b'Archive comment preserved'
  for name,data in expected.items():
   info=zipfile.ZipInfo(name,(2001,2,3,4,5,6));info.compress_type=zipfile.ZIP_DEFLATED;info.extra=struct.pack('<HH',0xcafe,4)+b'blob';info.comment=b'Entry comment';info.external_attr=(0o100640<<16)|0x20
   z.writestr(info,data)
 archive=root/'OPAQUE.ZIP';archive.write_bytes(stream.getvalue());before,comment=raw(archive)
 run('a',archive,'ADDED.TXT');expected['ADDED.TXT']=addition;check(archive,expected)
 after,current=raw(archive);assert current==comment
 for n in before:assert before[n]==after[n],n
 snapshot=archive.read_bytes();assert 'metadata' in run('a',archive,'KEEP.TXT',good=False);assert archive.read_bytes()==snapshot
 run('d',archive,'DIR');del expected['DIR/'];del expected['DIR/A.TXT'];check(archive,expected)
 after,current=raw(archive);assert current==comment and after['KEEP.TXT']==before['KEEP.TXT'] and after['snow-\u96ea.txt']==before['snow-\u96ea.txt']
 # Non-ASCII names are retained; replacing a differently cased ASCII entry keeps its name/comment.
 archive=root/'CASE.ZIP'
 with zipfile.ZipFile(archive,'w') as z:
  info=zipfile.ZipInfo('keep.txt',(2002,1,1,0,0,0));info.comment=b'preserved comment';z.writestr(info,b'old')
 run('a',archive,'KEEP.TXT');check(archive,{'keep.txt':b'unchanged'})
 with zipfile.ZipFile(archive) as z:assert z.getinfo('keep.txt').comment==b'preserved comment'
 # Rename changes names only, including folder descendants and Unicode suffixes.
 archive=root/'RENAME.ZIP'
 expected={'DIR/':b'','DIR/A.TXT':first,'DIR/snow-\u96ea.txt':b'unicode','KEEP.TXT':b'keep'}
 with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
  z.comment=b'Rename comment'
  for name,data in expected.items():
   info=zipfile.ZipInfo(name,(2001,2,3,4,5,6));info.compress_type=zipfile.ZIP_DEFLATED;info.comment=b'unchanged entry comment';z.writestr(info,data)
 run('rn',archive,'dir','RENAMED')
 expected={('RENAMED'+n[3:] if n.startswith('DIR') else n):v for n,v in expected.items()};check(archive,expected)
 with zipfile.ZipFile(archive) as z:assert z.comment==b'Rename comment' and all(i.comment==b'unchanged entry comment' for i in z.infolist())
 snapshot=archive.read_bytes()
 for destination in ['KEEP.TXT','../UNSAFE']:
  run('rn',archive,'RENAMED',destination,good=False);assert archive.read_bytes()==snapshot
 run('rn',archive,'RENAMED/A.TXT','MOVED.TXT');expected['MOVED.TXT']=expected.pop('RENAMED/A.TXT');check(archive,expected)
 run('rn',archive,'MOVED.TXT','moved.txt');expected['moved.txt']=expected.pop('MOVED.TXT');check(archive,expected)
 # Validate damaged data even if that entry would be deleted or replaced.
 archive=root/'BAD.ZIP';run('a',archive,'-m0=copy','KEEP.TXT');b=bytearray(archive.read_bytes());n,x=struct.unpack_from('<HH',b,26);b[30+n+x]^=1;archive.write_bytes(b)
 for args in [('d',archive,'KEEP.TXT'),('a',archive,'KEEP.TXT')]:run(*args,good=False);assert archive.read_bytes()==bytes(b)
 # Detection of existing ZIPs is independent of the suffix.
 archive=root/'ODD.7Z';archive.write_bytes((root/'deflate.ZIP').read_bytes());run('a',archive,'KEEP.TXT');check(archive,{'ADDED.TXT':addition,'KEEP.TXT':b'unchanged'})
 snapshot=archive.read_bytes();run('a',archive,'-m0=lzma','KEEP.TXT',good=False);assert archive.read_bytes()==snapshot
 print('PASS ZIP Store/Deflate add/replace/delete, descriptors, metadata/comments, Unicode, directories, empty archives, odd timestamps, collisions, exact retention and damaged-original protection')
