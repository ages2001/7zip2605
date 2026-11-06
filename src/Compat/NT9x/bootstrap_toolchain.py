"""Fetch hash-locked build inputs into a private directory; never install globally.

Host: Debian 13 amd64 with gcc/g++, make, binutils, git, Python 3 (venv),
bison, flex, gawk, libgmp-dev, libmpfr6, libmpc3, libisl23, zlib1g, libzstd1.
NT9X_DOWNLOAD_CACHE may point at an existing directory of Debian packages.
"""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, urllib.request
source = Path(__file__).resolve().parents[2]
base = Path(os.environ.get('NT9X_TOOLS', '/workspace/nt9x-tools')).resolve()
lock = json.loads((source / 'Compat/NT9x/toolchain-lock.json').read_text())
cache = base / 'downloads'
cache.mkdir(parents=True, exist_ok=True)
external = Path(os.environ.get('NT9X_DOWNLOAD_CACHE', str(cache)))
def run(args, cwd=None):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)
def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()
def fetch(item, name):
    path = cache / name
    if not path.exists() and (external / name).is_file():
        shutil.copyfile(external / name, path)
    if not path.exists():
        temporary = path.with_suffix(path.suffix + '.part')
        with urllib.request.urlopen(item['url'], timeout=120) as response, temporary.open('wb') as out:
            shutil.copyfileobj(response, out)
        if digest(temporary) != item['sha256']:
            temporary.unlink()
            raise RuntimeError('SHA256 mismatch: ' + name)
        temporary.replace(path)
    if digest(path) != item['sha256']:
        raise RuntimeError('SHA256 mismatch: ' + name)
    return path
for package in lock['debian_packages']:
    archive = fetch(package, package['file'])
    run(['dpkg-deb', '-x', archive, base / 'root'])
# Toolchain symlinks reference the matching Debian 13 host libraries.
for name in ['libmpfr.so.6.2.2', 'libmpc.so.3.3.1']:
    host = Path('/usr/lib/x86_64-linux-gnu') / name
    if not host.exists():
        raise RuntimeError('Missing Debian 13 host library: ' + str(host))
    target = base / 'root/usr/lib/x86_64-linux-gnu' / name
    if not target.exists():
        target.symlink_to(host)
mingw = base / 'mingw-src'
if not mingw.exists():
    run(['git', 'clone', os.environ.get('NT9X_MINGW_SOURCE', 'https://github.com/mingw-w64/mingw-w64.git'), mingw])
if subprocess.check_output(['git', '-C', str(mingw), 'status', '--porcelain'], text=True).strip():
    raise RuntimeError('MinGW source must have no local changes')
run(['git', '-C', mingw, 'checkout', '--detach', lock['mingw_source_commit']])
gcc = base / 'gcc-source/gcc-14.2.0'
if not gcc.exists():
    gcc.parent.mkdir(parents=True, exist_ok=True)
    run(['tar', '-xJf', base / 'root/usr/src/gcc-14/gcc-14.2.0-dfsg.tar.xz', '-C', gcc.parent])
aliases = base / 'include-aliases'
aliases.mkdir(exist_ok=True)
for name in ['CommCtrl.h','HtmlHelp.h','MAPI.h','NTSecAPI.h','OleCtl.h','Psapi.h','ShObjIdl.h','ShlGuid.h','ShlObj.h','Shlwapi.h','TlHelp32.h','WinUser.h','WinVer.h','Windows.h']:
    link = aliases / name
    if not link.exists():
        link.symlink_to(base / 'root/usr/share/mingw-w64/include' / name.lower())
installer = fetch(lock['official_manual_installer'], '7z2605-official.exe')
seven = base / 'root/usr/lib/7zip/7z'
manual = base / 'manual'
manual.mkdir(exist_ok=True)
run([seven, 'x', '-y', installer, '7-zip.chm', '-o' + str(manual)])
run([seven, 'x', '-y', manual / '7-zip.chm', '-o' + str(manual / 'html')])
if not (base / 'python/bin/python').exists():
    run(['python3', '-m', 'venv', base / 'python'])
run([base / 'python/bin/python', '-m', 'pip', 'install', 'pefile==2024.8.26'])
run(['bash', source / 'Compat/NT9x/build_runtime.sh'])
