"""Assemble the combined NT/9x + Windows 3.1x/Win32s 7zsetw32.exe candidate.

Inputs:
  --nt      directory with the NT/9x production build (build_all.sh output)
  --win32s  directory with the Win32s experiment build (build_experiment.py
            --gui-unicode-bridge output, relinked with win32s_registry.o and
            win32s_system.o)
  --output  new empty candidate directory

The setup and uninstall stubs are compiled from current source and linked with
the Win32s-capable runtime objects; every Win32s path in them is selected at run
time, so the same stub runs on NT 3.1-4.0, Windows 95 and Win32s.
Payload layout (non-solid LZMA, no executable filter):
  common files     Uninst.exe readme.txt History.txt License.txt
  nt/              7z.exe 7zFM.exe 7zG.exe 7-zip.dll 7zWF.dll 7z.sfx 7zCon.sfx
  w32s/            7z.exe 7zFM.exe 7zG.exe (Win32s hosted console and GUI)
The installer strips the platform prefix and ignores the other platform.
Nothing is published. Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

p = argparse.ArgumentParser()
p.add_argument('--nt', type=Path, required=True)
p.add_argument('--win32s', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--tools', type=Path, default=Path(os.environ.get('NT9X_TOOLS', '/workspace/nt9x-tools')))
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
nt, w32s, out, tools = a.nt.resolve(), a.win32s.resolve(), a.output.resolve(), a.tools.resolve()
cc = tools / 'root/usr/bin/i686-w64-mingw32-gcc-win32'
strip = tools / 'root/usr/bin/i686-w64-mingw32-strip'
seven = tools / 'root/usr/lib/7zip/7z'
out.mkdir(parents=True, exist_ok=False)

def run(args, **kw):
    subprocess.run([str(x) for x in args], check=True, **kw)

flags = ['-march=i386', '-mcrtdll=crtdll', '-Os', '-DZ7_LEGACY_WIN32', '-DZ7_NO_UNICODE',
         '-DZ7_WIN32_WINNT_MIN=0x030A', '-D_WIN32_WINNT=0x0400', '-DWINVER=0x0400', '-DZ7_LZMA_SIZE_OPT',
         '-DZ7_NO_METHOD_LZMA2', '-DZ7_NO_METHODS_FILTERS', '-DZ7_USE_NATIVE_BRANCH_FILTER', '-DZ7_EXTRACT_ONLY']
adapters = [w32s / 'crtexewin.o', w32s / 'registry.o', w32s / 'system.o', w32s / 'environment.o']
for f in adapters:
    if not f.is_file():
        raise SystemExit('missing Win32s adapter object: ' + str(f))
stubs = {}
for name in ['7zipInstall', '7zipUninstall']:
    dest = out / 'build' / name
    dest.mkdir(parents=True)
    sources = [root / f'C/Util/{name}/{name}.c'] + [root / f'Compat/NT9x/{n}.c' for n in ['setup_win95', 'setup_shortcuts', 'setup_paths', 'setup_winfile']]
    if name == '7zipInstall':
        sources += [root / f'C/{n}.c' for n in ['7zAlloc', '7zArcIn', '7zBuf', '7zFile', '7zDec', '7zStream', 'Bcj2', 'Bra', 'CpuArch', 'DllSecur', 'LzmaDec', '7zCrc', '7zCrcOpt']]
    objects = []
    for source in sources:
        obj = dest / (source.stem + '.o')
        run([cc, *flags, '-c', source, '-o', obj])
        objects.append(obj)
    rsp = dest / 'objects.rsp'
    rsp.write_text('\n'.join(map(str, objects + adapters)) + '\n')
    resource = dest / 'resource.o'
    run([tools / 'root/usr/bin/i686-w64-mingw32-windres', '--preprocessor=' + str(cc), '--preprocessor-arg=-E',
         '--preprocessor-arg=-xc', '--preprocessor-arg=-DRC_INVOKED', '-I', tools / 'include-aliases', 'resource.rc', resource],
        cwd=root / f'C/Util/{name}')
    exe = dest / (name + '.exe')
    env = dict(os.environ, NT9X_TOOLS=str(tools), NT9X_BUILD=str(w32s), NT9X_PROCESS_OBJECT='', NT9X_GUI_OUTPUT=str(exe),
               NT9X_GUI_OBJECTS=str(rsp), NT9X_GUI_RESOURCE=str(resource), NT9X_GUI_MAP=str(dest / 'link.map'))
    run(['bash', root / 'Compat/NT9x/link_gui_i386.sh'], env=env)
    stubs[name] = exe

payload = out / 'payload'
(payload / 'nt').mkdir(parents=True)
(payload / 'w32s').mkdir()
binaries = {
    'Uninst.exe': stubs['7zipUninstall'],
    'nt/7z.exe': nt / '7zz-i386.exe', 'nt/7zFM.exe': nt / '7zFM-i386.exe', 'nt/7zG.exe': nt / '7zG.exe',
    'nt/7-zip.dll': nt / '7-zip.dll', 'nt/7zWF.dll': nt / '7zWF.dll', 'nt/7z.sfx': nt / '7z.sfx', 'nt/7zCon.sfx': nt / '7zCon.sfx',
    'w32s/7z.exe': w32s / 'CLIH86.EXE', 'w32s/7zFM.exe': w32s / 'FMFULL.EXE', 'w32s/7zG.exe': w32s / 'G86.EXE',
}
for target, source in binaries.items():
    run([strip, '-o', payload / target, source])
shutil.copyfile(root / 'Compat/NT9x/README.txt', payload / 'readme.txt')
shutil.copyfile(root / 'DOC/src-history.txt', payload / 'History.txt')
licenses = [root / 'DOC/License.txt', root / 'DOC/copying.txt', root / 'DOC/unRarLicense.txt',
            tools / 'mingw-src/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt',
            tools / 'gcc-source/gcc-14.2.0/COPYING3', tools / 'gcc-source/gcc-14.2.0/COPYING.RUNTIME']
with (payload / 'License.txt').open('w', encoding='utf-8', newline='\r\n') as f:
    f.write('7-Zip NT/9x and Win32s compatibility additions: Copyright (c) 2026. LGPL-2.1-or-later.\n\n')
    for lic in licenses:
        f.write('\n===== ' + lic.name + ' =====\n\n')
        f.write(lic.read_text())
archive = out / 'PAYLOAD.7Z'
# Non-solid: the setup decoder unpacks each folder into memory; Win32s guests
# may have 16 MiB. The small decoder has no BCJ filter.
run([seven, 'a', '-t7z', '-m0=lzma', '-mf=off', '-ms=off', '-md=8m', '-mx7', archive, '.'], cwd=payload, stdout=subprocess.DEVNULL)
setup = out / '7zsetw32.exe'
run([strip, '-o', setup, stubs['7zipInstall']])
setup.write_bytes(setup.read_bytes() + archive.read_bytes())
manifest = {str(f.relative_to(payload)).replace('\\', '/'): {'sha256': hashlib.sha256(f.read_bytes()).hexdigest(), 'bytes': f.stat().st_size}
            for f in sorted(payload.rglob('*')) if f.is_file()}
manifest['7zsetw32.exe'] = {'sha256': hashlib.sha256(setup.read_bytes()).hexdigest(), 'bytes': setup.stat().st_size}
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('7zsetw32.exe', setup.stat().st_size, 'bytes,', len(manifest) - 1, 'payload files')
