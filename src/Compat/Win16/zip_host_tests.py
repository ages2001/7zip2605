#!/usr/bin/env python3
"""Independent ZIP fixtures and malformed-directory checks for native adapter."""
import argparse,io,pathlib,random,struct,subprocess,tempfile,zipfile
p=argparse.ArgumentParser();p.add_argument('console');a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp);src=root/'TEST.ZIP';counter=0
 def run(data,good,extract=False):
  global counter
  counter+=1;src.write_bytes(data);dest=root/f'out{counter}'
  args=[a.console,'x' if extract else 't',str(src)]+([str(dest)] if extract else [])
  r=subprocess.run(args,capture_output=True,text=True)
  assert (r.returncode==0)==good,(r.returncode,r.stdout,r.stderr)
  assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
  if extract and not good:assert not dest.exists()
  return dest
 data=random.Random(36).randbytes(40000)*3+b'Hello ZIP\r\n'*10000
 for method in [zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED]:
  for descriptor in [False,True]:
   class NonSeek(io.BytesIO):
    def seekable(self):return False
    def seek(self,*args):raise OSError('streaming')
   buf=NonSeek() if descriptor else io.BytesIO()
   with zipfile.ZipFile(buf,'w',compression=method) as z:
    z.writestr('FIRST.BIN',data);z.writestr('DIR/',b'');z.writestr('DIR/ZERO.TXT',b'');z.comment=b'comment PK\x05\x06'+b'Z'*300
   packed=buf.getvalue();run(packed,True);out=run(packed,True,True)
   assert (out/'FIRST.BIN').read_bytes()==data and (out/'DIR/ZERO.TXT').read_bytes()==b''
   for cut in [1,15,22,100]:run(packed[:-cut],False)
   damaged=bytearray(packed);damaged[30]^=1;run(damaged,False)
   central=packed.index(b'PK\x01\x02');damaged=bytearray(packed);struct.pack_into('<I',damaged,central+42,0xffffff00);run(damaged,False)
 for name in ['../BAD.TXT','C:/BAD.TXT','CON','A/../../X','x.txt']:
  buf=io.BytesIO()
  with zipfile.ZipFile(buf,'w',compression=8) as z:
   z.writestr(name,b'test')
   if name=='x.txt':z.writestr('X.TXT',b'duplicate')
  run(buf.getvalue(),False,True)
 buf=io.BytesIO()
 with zipfile.ZipFile(buf,'w') as z:pass
 run(buf.getvalue(),True)
 # UTF-8 names are retained for listing; unsupported DOS names are rejected.
 buf=io.BytesIO()
 with zipfile.ZipFile(buf,'w',compression=8) as z:z.writestr('snow-\u96ea.txt',b'Unicode')
 run(buf.getvalue(),True);run(buf.getvalue(),False,True)
 # Unknown extra metadata is preserved in the source and triggers a warning.
 buf=io.BytesIO()
 with zipfile.ZipFile(buf,'w') as z:
  info=zipfile.ZipInfo('EXTRA.TXT',(2000,1,1,0,0,0));info.extra=b'\xfe\xca\x02\x00AB';z.writestr(info,b'extra')
 packed=buf.getvalue();run(packed,True);src.write_bytes(packed)
 r=subprocess.run([a.console,'x',str(src),str(root/'extra')],capture_output=True,text=True)
 assert r.returncode==0 and 'archive-specific metadata' in r.stdout,r.stdout
 # Malformed extra TLV lengths must not be silently ignored.
 damaged=bytearray(packed);struct.pack_into('<H',damaged,30+len('EXTRA.TXT')+2,100);run(damaged,False)
 # DOS 8.3 collisions and directory/file conflicts fail before output.
 for paths in [('DIR','DIR/F.TXT'),('FILE.TXT','file.txt')]:
  buf=io.BytesIO()
  with zipfile.ZipFile(buf,'w') as z:
   for name in paths:z.writestr(name,b'x')
  run(buf.getvalue(),False,True)
 print('PASS ZIP stored/Deflate, descriptors, directories, empty files, comments, bounds, names and extraction safety')
