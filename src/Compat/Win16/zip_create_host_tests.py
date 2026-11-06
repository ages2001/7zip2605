#!/usr/bin/env python3
"""Interoperability and source-preservation tests for native ZIP creation."""
import argparse,os,pathlib,subprocess,tempfile,zipfile,random,struct
p=argparse.ArgumentParser();p.add_argument('--console',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp);data=bytes(range(256))*600+b'ZIP creation\r\n'*5000
 (root/'DATA.BIN').write_bytes(data);(root/'RANDOM.BIN').write_bytes(random.Random(39).randbytes(140000));(root/'EMPTY.TXT').write_bytes(b'')
 for name in ['DATA.BIN','RANDOM.BIN','EMPTY.TXT']:os.utime(root/name,(946684801,946684801))
 os.chmod(root/'DATA.BIN',0o444)
 def run(args,good=True):
  r=subprocess.run([a.console,*args],cwd=root,capture_output=True,text=True);assert (r.returncode==0)==good,(r.stdout,r.stderr);assert not r.stderr,r.stderr;return r.stdout
 for method in ['copy','deflate']:
  path=root/(method+'.ZIP');run(['a',str(path),'-m0='+method,'DATA.BIN','RANDOM.BIN','EMPTY.TXT'])
  snapshot=path.read_bytes()
  with zipfile.ZipFile(path) as z:
   assert z.testzip() is None and z.namelist()==['DATA.BIN','RANDOM.BIN','EMPTY.TXT']
   for name in z.namelist():
    assert z.read(name)==(root/name).read_bytes();info=z.getinfo(name);assert info.compress_type==(0 if method=='copy' else 8)
    assert info.extra==b'UT\x05\x00\x01'+struct.pack('<I',946684801)
   assert z.getinfo('DATA.BIN').external_attr&1
   if method=='deflate':assert z.getinfo('DATA.BIN').compress_size<len(data)//10
  out=root/(method+'OUT');run(['x',str(path),str(out)])
  for name in ['DATA.BIN','RANDOM.BIN','EMPTY.TXT']:
   assert (out/name).read_bytes()==(root/name).read_bytes();assert int((out/name).stat().st_mtime)==946684801
  r=subprocess.run([a.sevenzip,'t',str(path)],capture_output=True);assert r.returncode==0,(r.stdout,r.stderr)
  protected=root/'PROTECT.ZIP';protected.write_bytes(b'Unrelated existing file')
  run(['a',str(protected),'DATA.BIN'],False);assert protected.read_bytes()==b'Unrelated existing file'
  for args in [['DATA.BIN','DATA.BIN'],['MISSING.TXT'],['-m0=lzma','DATA.BIN']]:
   bad=root/'BAD.ZIP';run(['a',str(bad),*args],False);assert not bad.exists()
  assert (root/'DATA.BIN').read_bytes()==data
 run(['a','EXPLICIT.BIN','-tzip','DATA.BIN']);assert zipfile.is_zipfile(root/'EXPLICIT.BIN')
 print('PASS Store/Deflate ZIP creation, empty/random/large inputs, independent zipfile and 7-Zip checks, exact timestamps, read-only attributes, existing destinations and rejected inputs')
