#!/usr/bin/env python3
"""Exercise CLI selection, recursive files, single-pass digests and errors."""
import argparse,pathlib,subprocess,tempfile,re
p=argparse.ArgumentParser();p.add_argument('--console',required=True);p.add_argument('--probe',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp);(root/'TREE/SUB').mkdir(parents=True);(root/'TREE/EMPTY').mkdir();(root/'TREE/A.TXT').write_bytes(b'abc');(root/'TREE/SUB/B.BIN').write_bytes(bytes(range(256))*512);(root/'TREE/ZERO.TXT').touch()
 def run(args,good=True):
  r=subprocess.run([a.console,*args],cwd=root,capture_output=True,text=True);assert (r.returncode==0)==good,(r.stdout,r.stderr);assert not r.stderr,r.stderr;return r.stdout
 out=run(['h','-scrc*','TREE']);rows=re.findall(r'^(\S+) ([A-F0-9]+) (\d+) (.+)$',out,re.M);assert len(rows)==33
 expected={}
 for f in (root/'TREE').rglob('*'):
  if f.is_file():
   r=subprocess.run([a.probe,str(f)],capture_output=True,text=True,check=True);expected[str(f.relative_to(root))]=dict(line.split() for line in r.stdout.splitlines())
 for method,digest,size,name in rows:assert digest==expected[name][method] and int(size)==(root/name).stat().st_size,(method,name)
 rows=re.findall(r'^(\S+) ([A-F0-9]+) (\d+) (.+)$',run(['h','-scrc=SHA256','-scrcCRC32','TREE']),re.M);assert len(rows)==6 and {r[0] for r in rows}=={'SHA256','CRC32'}
 assert len(re.findall(r'^CRC32 ',run(['h','TREE/A.TXT']),re.M))==1
 for args in [['h','-scrcUNKNOWN','TREE'],['h','MISSING.TXT'],['h','-scrc=','TREE']]:assert 'Everything is Ok' not in run(args,False)
 print('PASS CLI h, all 11 methods, repeated/optional-equals selection, default CRC32, recursive files and explicit nonzero errors')
