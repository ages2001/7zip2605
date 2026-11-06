#!/usr/bin/env python3
"""Independent pathless extraction checks, including collisions and rollback."""
import argparse,pathlib,subprocess,tempfile,zipfile,os
p=argparse.ArgumentParser();p.add_argument('--console',required=True);p.add_argument('--faults',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp);data=bytes(range(256))*600
 def run(*args,good=True):
  r=subprocess.run([a.console,*map(str,args)],cwd=root,capture_output=True,text=True)
  assert (r.returncode==0)==good,(args,r.stdout,r.stderr)
  assert not r.stderr,r.stderr
  return r.stdout
 files={'A/FIRST.BIN':data,'B/SECOND.BIN':b'second'*17000,'ZERO.TXT':b'','snow-\u96ea/SAFE.TXT':b'safe basename'}
 for name,content in files.items():
  f=root/name;f.parent.mkdir(exist_ok=True);f.write_bytes(content);os.utime(f,(946684800,946684800))
 (root/'EMPTY').mkdir()
 r=subprocess.run([a.sevenzip,'a','-t7z','-m0=LZMA2:d=32k','FLAT.7Z','A','B','ZERO.TXT','snow-\u96ea','EMPTY'],cwd=root,capture_output=True);assert r.returncode==0,r.stdout
 with zipfile.ZipFile(root/'FLAT.ZIP','w',compression=zipfile.ZIP_DEFLATED) as z:
  for name,content in files.items():z.writestr(name,content)
  z.writestr('EMPTY/',b'')
 for ext in ('7Z','ZIP'):
  archive=root/('FLAT.'+ext);before=archive.read_bytes();out=root/('OUT'+ext)
  text=run('e',archive,out);assert 'directory metadata (paths were omitted)' in text
  assert {f.name:f.read_bytes() for f in out.iterdir()}=={pathlib.Path(n).name:c for n,c in files.items()}
  assert archive.read_bytes()==before
  run('e',archive,out,good=False);assert {f.name:f.read_bytes() for f in out.iterdir()}=={pathlib.Path(n).name:c for n,c in files.items()}
  run('x',archive,root/'FULL',good=False);assert not (root/'FULL').exists()
  r=subprocess.run([a.faults,str(archive),'e'],cwd=root,capture_output=True,text=True);assert r.returncode==0,(r.stdout,r.stderr);assert not r.stderr
  assert archive.read_bytes()==before
 for names in [('A/FILE.TXT','B/file.txt'),('A/CON','B/B.TXT'),('A/TOO-LONG-NAME.TXT','B/B.TXT')]:
  archive=root/'BAD.ZIP'
  with zipfile.ZipFile(archive,'w') as z:
   for name in names:z.writestr(name,b'bytes')
  run('e',archive,root/'BADOUT',good=False);assert not (root/'BADOUT').exists()
 # Dangerous parent components are discarded; they are never interpreted as paths.
 archive=root/'PARENT.ZIP'
 with zipfile.ZipFile(archive,'w') as z:z.writestr('../ABSENT/SAFE.TXT',b'contained')
 run('e',archive,root/'SAFEOUT');assert (root/'SAFEOUT/SAFE.TXT').read_bytes()==b'contained';assert not (root.parent/'ABSENT').exists()
 print('PASS pathless 7z/ZIP extraction, Unicode parents, discarded paths, empty files/folders, collisions, existing-output protection, partial write/close/cancel rollback and unchanged sources')
