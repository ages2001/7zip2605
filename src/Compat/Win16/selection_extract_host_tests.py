#!/usr/bin/env python3
"""Independent selected-output contents and rollback checks for the shared engine."""
import argparse,pathlib,tempfile,subprocess,zipfile,re,hashlib,shutil,os
p=argparse.ArgumentParser();p.add_argument('--probe',required=True);p.add_argument('--sevenzip',required=True);a=p.parse_args();passed=0
with tempfile.TemporaryDirectory()as tmp:
 root=pathlib.Path(tmp);files={'A/FIRST.BIN':bytes((i*17+3)&255 for i in range(12291)),'B/SECOND.BIN':bytes(i&255 for i in range(8193)),'ZERO.TXT':b'','LONG-NAME-IGNORE.TXT':b'not selected','A/Clash.txt':b'first','B/CLASH.TXT':b'second'}
 for name,data in files.items():
  f=root/name;f.parent.mkdir(exist_ok=True);f.write_bytes(data)
 (root/'EMPTY').mkdir()
 with zipfile.ZipFile(root/'SEL80.ZIP','w',compression=zipfile.ZIP_DEFLATED)as z:
  for name,data in files.items():z.writestr(name,data)
  z.writestr('EMPTY/',b'')
 cp=subprocess.run([a.sevenzip,'a','-t7z','-m0=LZMA2:d=16k','SEL80.7Z','A','B','ZERO.TXT','EMPTY','LONG-NAME-IGNORE.TXT'],cwd=root,capture_output=True);assert cp.returncode==0,cp.stdout
 def run(arc,mask,flat=0,fault='ok',allocation=0,expected=0,existing=False):
  global passed
  out=root/'OUT'
  if out.exists()and not existing:shutil.rmtree(out)
  before=arc.read_bytes();cp=subprocess.run([a.probe,str(arc),str(out),str(flat),mask,str(allocation),fault],cwd=root,capture_output=True,text=True)
  assert cp.returncode==0 and not cp.stderr,(cp.returncode,cp.stdout,cp.stderr)
  match=re.search(r'RESULT (\d+) CALLS (\d+)',cp.stdout);assert match,cp.stdout
  result,calls=map(int,match.groups());assert result==expected,(arc.name,mask,flat,fault,allocation,expected,cp.stdout)
  assert arc.read_bytes()==before
  if result and not existing:assert not out.exists(),cp.stdout
  passed+=1;return out,calls,cp.stdout
 def contents(out):return {str(f.relative_to(out)):f.read_bytes()for f in out.rglob('*')if f.is_file()}
 for ext in ['ZIP','7Z']:
  arc=root/('SEL80.'+ext)
  listed=subprocess.run([a.sevenzip,'l','-ba','-slt',str(arc)],capture_output=True,text=True,check=True).stdout
  names=[m.group(1).rstrip('/')for m in re.finditer(r'^Path = (.+)$',listed,re.M)]
  def select(*chosen):return ','.join(str(names.index(n))for n in chosen)
  subset=select('A/FIRST.BIN','ZERO.TXT','EMPTY');out,calls,_=run(arc,subset)
  assert contents(out)=={'A/FIRST.BIN':files['A/FIRST.BIN'],'ZERO.TXT':b''}and(out/'EMPTY').is_dir();assert not(out/'B').exists()
  for n in range(1,calls+1):run(arc,subset,allocation=n,expected=2)
  for fault,expected in [('write',9),('close',9),('cancel',10)]:run(arc,subset,fault=fault,expected=expected)
  out,_,_=run(arc,select('A/FIRST.BIN','B/SECOND.BIN'),flat=1)
  assert contents(out)=={'FIRST.BIN':files['A/FIRST.BIN'],'SECOND.BIN':files['B/SECOND.BIN']}
  out,_,_=run(arc,select('A/Clash.txt'),flat=1);assert contents(out)=={'CLASH.TXT':b'first'}
  run(arc,select('A/Clash.txt','B/CLASH.TXT'),flat=1,expected=4)
  run(arc,select('LONG-NAME-IGNORE.TXT'),expected=4);run(arc,'none',expected=5);run(arc,'short',expected=5)
  out,_,_=run(arc,subset);(out/'KEEP.TXT').write_bytes(b'user file');saved=contents(out);run(arc,subset,expected=9,existing=True);assert contents(out)==saved
  print('PASS',ext,'selected solid/ZIP output, empty directory, excluded unsafe name, collisions, every allocation failure, write/close/cancel rollback and unchanged source')
 # An unselected corrupt member is still detected by the conservative whole-archive integrity pass.
 arc=root/'CRC80.ZIP'
 with zipfile.ZipFile(arc,'w',compression=zipfile.ZIP_STORED)as z:z.writestr('GOOD.TXT',b'good');z.writestr('BAD.TXT',b'brokenpayload')
 data=bytearray(arc.read_bytes());at=data.index(b'brokenpayload');data[at]^=1;arc.write_bytes(data);run(arc,'0',expected=3)
 print('PASS',passed,'selected extraction/rollback cases; all archives unchanged')
