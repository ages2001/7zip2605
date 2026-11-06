"""Package the tested visual revision, retaining validated setup/core evidence."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,tempfile,zipfile
p=argparse.ArgumentParser(description=__doc__)
for name in ['candidate','setup_base','behavior_base','output']:p.add_argument(name,type=Path)
a=p.parse_args();source=Path(__file__).resolve().parents[2];results=source/'Compat/NT9x/test-results';out=a.output.resolve()
if out.exists() or out.is_relative_to(source):raise SystemExit('Choose a new output outside source')
def sha(data):return hashlib.sha256(data).hexdigest()
def payload(path):return {p.name.lower():sha(p.read_bytes()) for p in (path/'payload').iterdir() if p.is_file()}
current,base=payload(a.candidate),payload(a.setup_base)
if set(current)!=set(base) or {n for n in current if current[n]!=base[n]}!={'7zfm.exe'}:raise SystemExit('Changes outside tested File Manager revision')
installer=(a.candidate/'SETUP.EXE').read_bytes();oldInstaller=(a.setup_base/'SETUP.EXE').read_bytes();archive=(a.candidate/'PAYLOAD.7Z').read_bytes();oldArchive=(a.setup_base/'PAYLOAD.7Z').read_bytes()
if not installer.endswith(archive) or not oldInstaller.endswith(oldArchive) or installer[:-len(archive)]!=oldInstaller[:-len(oldArchive)]:raise SystemExit('Installer stub changed: fresh setup suite required')
for system in ['nt31','nt351','nt4','win95']:
 directory=results/'visual25'/system;r=json.loads((directory/'manifest.json').read_text())
 if not r['installed_matches_payload'] or r['installed_files_sha256']!=current:raise SystemExit('Visual test binary mismatch: '+system)
 for name in ['CTRLTEST.TXT','LISTTEST.TXT','VISUAL25.TXT','ICONTEST.TXT','ABOUT24.TXT','GUIADV.TXT','GUIHELP.TXT','ASSTEST.TXT']:
  text=(directory/name).read_text()
  if not text.strip().endswith('ALL') or any(s.startswith('FAIL') for s in text.splitlines()):raise SystemExit('Failed visual test: '+system+'/'+name)
 for name,digest in r['captured_files_sha256'].items():
  if sha((directory/name).read_bytes())!=digest:raise SystemExit('Changed visual evidence: '+name)
embedded=json.loads((results/'visual25/installer.json').read_text())
if not embedded['all_payload_files_embedded'] or embedded['installer_sha256']!=sha(installer) or {n.lower():v for n,v in embedded['files_sha256'].items()}!=current:raise SystemExit('Embedded payload mismatch')
r=json.loads((results/'visual25/resources.json').read_text())
if not r['only_about_changed'] or r['missing'] or r['reference_sha256']!=payload(a.behavior_base)['7zfm.exe'] or r['current_sha256']!=current['7zfm.exe']:raise SystemExit('GUI resource audit mismatch')
# Validate the prior core/setup suites with their exact binaries. Reuse the
# original source inclusion and exclusion policy; never distribute this base ZIP.
with tempfile.TemporaryDirectory(prefix='z7-visual-package-') as temp:
 basezip=Path(temp)/'base.zip'
 subprocess.run([sys.executable,str(source/'Compat/NT9x/package_distribution.py'),str(a.setup_base.resolve()),str(basezip),'--revision-base',str(a.behavior_base.resolve())],check=True,stdout=subprocess.DEVNULL)
 with zipfile.ZipFile(basezip) as z:files={n:z.read(n) for n in z.namelist() if n!='SHA256SUMS.txt'}
 for file in (a.candidate/'payload').iterdir():files['bin/'+file.name]=file.read_bytes()
 files['bin/SETUP.EXE']=installer
 files['SHA256SUMS.txt']=''.join(sha(data)+'  '+name+'\n' for name,data in sorted(files.items())).encode()
 out.parent.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(out,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name,data in sorted(files.items()):z.writestr(name,data)
 with zipfile.ZipFile(out) as z:
  if z.testzip():raise SystemExit('ZIP CRC failure')
  for line in z.read('SHA256SUMS.txt').decode().splitlines():
   digest,name=line.split('  ',1)
   if sha(z.read(name))!=digest:raise SystemExit('ZIP digest failure: '+name)
print(str(out));print('Files:',len(files),'Bytes:',out.stat().st_size);print('SHA256:',sha(out.read_bytes()))
