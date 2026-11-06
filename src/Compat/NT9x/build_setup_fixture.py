"""Assemble an internal setup test payload, never a release artifact."""
from pathlib import Path
import os,subprocess,sys,shutil
base=Path(os.environ.get('NT9X_BUILD','/workspace/nt9x-build'))
tools=Path(os.environ.get('NT9X_TOOLS','/workspace/nt9x-tools'))
out=Path(sys.argv[1]).resolve();payload=out/'payload';payload.mkdir(parents=True,exist_ok=True)
strip=str(tools/'root/usr/bin/i686-w64-mingw32-strip');seven=str(tools/'root/usr/lib/7zip/7z')
for source,target in {'7zz-i386.exe':'7z.exe','7zFM-i386.exe':'7zFM.exe','7zG.exe':'7zG.exe','7-zip.dll':'7-zip.dll','7zWF.dll':'7zWF.dll','7z.sfx':'7z.sfx','7zCon.sfx':'7zCon.sfx','7zipUninstall-i386.exe':'Uninst.exe'}.items():subprocess.check_call([strip,'-o',str(payload/target),str(base/source)])
source=Path(__file__).resolve().parents[2]
shutil.copyfile(source/'Compat/NT9x/README.txt',payload/'readme.txt')
shutil.copyfile(source/'DOC/src-history.txt',payload/'History.txt')
licenses=[source/'DOC/License.txt',source/'DOC/copying.txt',source/'DOC/unRarLicense.txt',tools/'mingw-src/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt',tools/'gcc-source/gcc-14.2.0/COPYING3',tools/'gcc-source/gcc-14.2.0/COPYING.RUNTIME']
with (payload/'License.txt').open('w',encoding='utf-8',newline='\r\n') as out_license:
    out_license.write('7-Zip NT/9x compatibility additions: Copyright (c) 2026. LGPL-2.1-or-later.\n\n')
    for license_file in licenses:
        out_license.write('\n===== '+license_file.name+' =====\n\n')
        out_license.write(license_file.read_text())
for name in ['SETAPI','SETTEST']:subprocess.check_call([strip,'-o',str(out/(name+'.EXE')),str(base/(name+'.EXE'))])
archive=out/'PAYLOAD.7Z'
if archive.exists():archive.unlink()
# The small upstream setup decoder deliberately excludes BCJ. Explicitly
# disable the archiver's automatic executable filter when making its payload.
subprocess.check_call([seven,'a','-t7z','-m0=lzma','-mf=off','-md=8m','-mx7',str(archive),'.'],cwd=payload,stdout=subprocess.DEVNULL)
exe=out/'SETUP.EXE';subprocess.check_call([strip,'-o',str(exe),str(base/'7zipInstall-i386.exe')]);exe.write_bytes(exe.read_bytes()+archive.read_bytes())
print('Test setup bytes:',exe.stat().st_size)
