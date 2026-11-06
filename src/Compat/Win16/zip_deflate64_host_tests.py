#!/usr/bin/env python3
"""Check method-9 ZIP creation/update with an independent decoder."""
import argparse,pathlib,tempfile,subprocess,zipfile,random,struct
p=argparse.ArgumentParser();p.add_argument('--console',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory()as tmp:
 root=pathlib.Path(tmp);archive=root/'NEW.ZIP';count=0
 def run(*args,good=True):
  global count
  r=subprocess.run([a.console,*map(str,args)],cwd=root,capture_output=True,text=True);count+=1
  assert not r.stderr and (r.returncode==0)==good,(args,r.stdout,r.stderr)
  return r.stdout
 def check(expected):
  with tempfile.TemporaryDirectory()as out:
   r=subprocess.run([a.sevenzip,'x','-o'+out,str(archive)],capture_output=True);assert r.returncode==0,r.stderr
   actual={str(f.relative_to(out)):f.read_bytes()for f in pathlib.Path(out).rglob('*')if f.is_file()};assert actual==expected,(actual.keys(),expected.keys())
 data=bytes(range(256))*800;new=random.Random(84).randbytes(80000)
 (root/'DATA.BIN').write_bytes(data);(root/'ZERO.TXT').write_bytes(b'');(root/'SUB').mkdir();(root/'SUB'/'TREE.TXT').write_bytes(b'tree')
 run('a',archive,'-m0=deflate64','DATA.BIN','ZERO.TXT','SUB');expected={'DATA.BIN':data,'ZERO.TXT':b'','SUB/TREE.TXT':b'tree'};check(expected)
 with zipfile.ZipFile(archive)as z:
  assert all(i.compress_type==(0 if i.is_dir()else 9)and i.extract_version==(20 if i.is_dir()else 21)for i in z.infolist())
 run('t',archive);run('x',archive,'OUT');assert(root/'OUT/DATA.BIN').read_bytes()==data
 snapshot=archive.read_bytes();run('a',root/'BAD.7Z','-m0=deflate64','DATA.BIN',good=False);assert not(root/'BAD.7Z').exists()
 run('a',archive,'-m0=lzma','DATA.BIN',good=False);assert archive.read_bytes()==snapshot
 (root/'DATA.BIN').write_bytes(new);run('a',archive,'-m0=deflate64','DATA.BIN');expected['DATA.BIN']=new;check(expected)
 with zipfile.ZipFile(archive)as z:assert all(i.compress_type==(0 if i.is_dir()else 9)for i in z.infolist())
 run('rn',archive,'DATA.BIN','RENAMED.BIN');expected['RENAMED.BIN']=expected.pop('DATA.BIN');check(expected)
 run('d',archive,'SUB');del expected['SUB/TREE.TXT'];check(expected)
 # Existing method-9 records remain byte-identical when another method adds a file.
 def record(name):
  packed=archive.read_bytes()
  with zipfile.ZipFile(archive)as z:
   i=z.getinfo(name);n,x=struct.unpack_from('<HH',packed,i.header_offset+26)
   return packed[i.header_offset:i.header_offset+30+n+x+i.compress_size]
 previous=record('RENAMED.BIN');(root/'NEW.TXT').write_bytes(b'ordinary deflate')
 run('a',archive,'-m0=deflate','NEW.TXT');expected['NEW.TXT']=b'ordinary deflate';check(expected);assert record('RENAMED.BIN')==previous
 for i in [258,259,65536]:
  (root/'NEW.TXT').write_bytes(b'A'*i);run('a',archive,'-m0=deflate64','NEW.TXT');expected['NEW.TXT']=b'A'*i;check(expected)
 run('d',archive,'ZERO.TXT','RENAMED.BIN','NEW.TXT');check({})
 print('PASS',count,'ZIP Deflate64 command cases: exact independent decoding, method/version headers, updates/rename/delete, invalid format protection and retained records')
