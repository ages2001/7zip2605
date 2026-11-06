#!/usr/bin/env python3
"""Independent recursive writer/update checks, including DOS name collisions."""
import argparse, os, pathlib, subprocess, tempfile, zipfile
p=argparse.ArgumentParser();p.add_argument('--console',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp)
 def run(args,good=True,cwd=root):
  r=subprocess.run([a.console,*map(str,args)],cwd=cwd,capture_output=True,text=True)
  assert (r.returncode==0)==good,(args,r.stdout,r.stderr)
  assert not r.stderr,r.stderr
  return r.stdout
 def files(path):return {str(p.relative_to(path)).upper():p.read_bytes() for p in path.rglob('*') if p.is_file()}
 def dirs(path):return {str(p.relative_to(path)).upper() for p in path.rglob('*') if p.is_dir()}
 tree=root/'TREE';(tree/'SUB'/'DEEP').mkdir(parents=True);(tree/'EMPTY').mkdir()
 (tree/'A.TXT').write_bytes(b'first\r\n'*9000);(tree/'SUB'/'B.BIN').write_bytes(bytes(range(256))*300);(tree/'SUB'/'DEEP'/'ZERO.TXT').touch()
 for q in tree.rglob('*'):os.utime(q,(946684801,946684801))
 os.utime(tree,(946684801,946684801));os.chmod(tree/'A.TXT',0o444)
 for fmt,method in [('7z','copy'),('7z','lzma'),('zip','copy'),('zip','deflate')]:
  path=tree/('SELF.'+fmt.upper());arg='-m0='+method
  run(['a',path,arg,tree]);original=path.read_bytes();expected=files(tree);expected.pop(path.name)
  out=root/'OUT';run(['x',path,out]);assert files(out/'TREE')==expected and dirs(out/'TREE')==dirs(tree)
  assert int((out/'TREE'/'A.TXT').stat().st_mtime)==int((tree/'A.TXT').stat().st_mtime)
  independent=root/'INDEP';r=subprocess.run([a.sevenzip,'x','-y','-o'+str(independent),str(path)],capture_output=True);assert r.returncode==0,(r.stdout,r.stderr)
  assert files(independent/'TREE')==expected and dirs(independent/'TREE')==dirs(tree)
  if fmt=='zip':
   with zipfile.ZipFile(path) as z:
    assert z.testzip() is None
    for n in ['TREE/','TREE/SUB/','TREE/SUB/DEEP/','TREE/EMPTY/']:assert z.getinfo(n).is_dir() and z.getinfo(n).external_attr&0x10 and z.getinfo(n).compress_type==0
  # Replacement and new subdirectories. Removed source files remain in archive.
  os.chmod(tree/'A.TXT',0o644);(tree/'A.TXT').write_bytes(b'replaced\r\n'*3000)
  (tree/'NEW').mkdir();(tree/'NEW'/'ADD.TXT').write_bytes(b'added');(tree/'NEW'/'VOID').mkdir()
  old=(tree/'SUB'/'B.BIN').read_bytes();(tree/'SUB'/'B.BIN').unlink()
  run(['a',path,arg,tree]);expected=files(tree);expected.pop(path.name);expected['SUB/B.BIN']=old
  out2=root/'OUT2';run(['x',path,out2]);assert files(out2/'TREE')==expected and dirs(out2/'TREE')==dirs(tree)
  assert not list(tree.glob('7R*'))
  before=path.read_bytes();run(['a',path,arg,tree,tree],False);assert path.read_bytes()==before
  import shutil
  for d in [out,out2,independent]:shutil.rmtree(d)
  path.unlink();shutil.rmtree(tree/'NEW');(tree/'SUB'/'B.BIN').write_bytes(old)
 # Dot source and self exclusion during update, including a hard link alias.
 dot=root/'DOT';dot.mkdir();(dot/'X.TXT').write_text('dot');(dot/'EMPTY').mkdir()
 run(['a','ALL.ZIP','.'],cwd=dot);os.link(dot/'ALL.ZIP',dot/'ALIAS.ZIP');run(['a','ALL.ZIP','.'],cwd=dot)
 with zipfile.ZipFile(dot/'ALL.ZIP') as z:assert set(z.namelist())=={'X.TXT','EMPTY/'}
 # Fail before opening destination for unrepresentable/special input trees.
 bad=root/'BAD';bad.mkdir()
 for name in ['LONGFILENAME.TXT','CON','A.TXT']:
  q=bad/name
  if name=='A.TXT':q.symlink_to(tree/'A.TXT')
  else:q.write_text('bad')
  for fmt in ['7z','zip']:run(['a','BAD.'+fmt,str(bad)],False);assert not (root/('BAD.'+fmt)).exists()
  q.unlink()
 (bad/'A.TXT').write_text('case');(bad/'a.txt').write_text('collision')
 run(['a','BAD.7Z',bad],False);assert not (root/'BAD.7Z').exists()
 # Exact limit is entries, including directories; overflow leaves no output.
 many=root/'MANY';many.mkdir()
 for i in range(255):(many/('%08d'%i)).touch()
 for fmt in ['7z','zip']:
  run(['a','LIMIT.'+fmt,many]);run(['t','LIMIT.'+fmt])
 (many/'OVERFLOW').touch()
 for fmt in ['7z','zip']:run(['a','OVER.'+fmt,many],False);assert not (root/('OVER.'+fmt)).exists()
 # Collision with an existing file used as a parent directory preserves archive.
 clash=root/'CLASH';clash.write_text('file');run(['a','CLASH.7Z',clash]);before=(root/'CLASH.7Z').read_bytes();clash.unlink();clash.mkdir();(clash/'X.TXT').write_text('child')
 run(['a','CLASH.7Z',clash],False);assert (root/'CLASH.7Z').read_bytes()==before
 print('PASS recursive 7z Copy/LZMA and ZIP Store/Deflate create/update; independent extraction; empty directories, metadata, self exclusion, collision/special-file rejection and entry limits')
