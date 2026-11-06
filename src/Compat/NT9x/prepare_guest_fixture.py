"""Assemble an isolated regression input directory. Never a release bin/."""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, sys
source=Path(__file__).resolve().parents[2]
build=Path(os.environ.get('NT9X_BUILD','/workspace/nt9x-build'))
tools=Path(os.environ.get('NT9X_TOOLS','/workspace/nt9x-tools'))
out=Path(sys.argv[1]).resolve()
metadata=Path(sys.argv[2]).resolve()
if out.exists():
    raise SystemExit('Choose a new output directory to preserve previous inputs')
subprocess.run([sys.executable,str(source/'Compat/NT9x/build_setup_fixture.py'),str(out)],check=True)
strip=tools/'root/usr/bin/i686-w64-mingw32-strip'
for name in ['GTEST','RTTEST','SETUNIC','GUIROUND','GUIDROP','COMPTEST','CTTEST','LVTEST','MNTEST','PHTEST','PSTEST','METATEST','OLETEST','SHELLTST','ICONTEST','MPRTEST','REGRESS']:
    subprocess.run([str(strip),'-o',str(out/(name+'.EXE')),str(build/(name+'.EXE'))],check=True)
# Copy the installed CLI verbatim: strip rewrites the PE timestamp.
shutil.copyfile(out/'payload/7z.exe',out/'7ZZ.EXE')
shutil.copyfile(build/'CLASS.DLL',out/'CLASS.DLL')
(out/'CODECS').mkdir()
shutil.copyfile(build/'plugin-fixture/TESTHASH.DLL',out/'CODECS/TESTHASH.DLL')
for name in ['META.WIM','DATA.TXT']:
    shutil.copyfile(metadata/name,out/name)
files={str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob('*') if p.is_file()}
(out/'manifest.json').write_text(json.dumps(files,indent=2)+'\n')
