"""Prepare a MinGW file-manager build using the upstream object manifests."""
from pathlib import Path
import re, os
root = Path(__file__).resolve().parents[2]
tools = Path(os.environ.get('NT9X_TOOLS', '/workspace/nt9x-tools'))
build = Path(os.environ.get('NT9X_BUILD', '/workspace/nt9x-build')) / 'gui-i386'
build.mkdir(parents=True, exist_ok=True)
groups = {}
for rel in ['CPP/7zip/Bundles/Format7zF/Arc.mak','CPP/7zip/UI/FileManager/FM.mak','CPP/7zip/Bundles/Fm/makefile']:
 text=(root/rel).read_text().replace('\\\n',' ')
 for line in text.splitlines():
  m=re.match(r'([A-Z0-9_]+_OBJS)\s*=\s*(.*)',line)
  if not m:continue
  group,value=m.groups()
  names=re.findall(r'\$O\\([\w]+)\.obj',value)
  if f'$({group})' in value:groups.setdefault(group,[]).extend(names)
  else:groups[group]=names
mapping={'COMMON_OBJS':'CPP/Common','WIN_OBJS':'CPP/Windows','WIN_CTRL_OBJS':'CPP/Windows/Control','7ZIP_COMMON_OBJS':'CPP/7zip/Common','UI_COMMON_OBJS':'CPP/7zip/UI/Common','FM_OBJS':'CPP/7zip/UI/FileManager','AGENT_OBJS':'CPP/7zip/UI/Agent','EXPLORER_OBJS':'CPP/7zip/UI/Explorer','GUI_OBJS':'CPP/7zip/UI/GUI','C_OBJS':'C','AR_OBJS':'CPP/7zip/Archive','AR_COMMON_OBJS':'CPP/7zip/Archive/Common','COMPRESS_OBJS':'CPP/7zip/Compress','CRYPTO_OBJS':'CPP/7zip/Crypto'}
for name,folder in [('7Z','7z'),('CAB','Cab'),('CHM','Chm'),('ISO','Iso'),('NSIS','Nsis'),('RAR','Rar'),('TAR','Tar'),('UDF','Udf'),('WIM','Wim'),('ZIP','Zip')]:mapping[name+'_OBJS']='CPP/7zip/Archive/'+folder
sources={root/'CPP/7zip/Compress/CodecExports.cpp'}
for group,names in groups.items():
 for name in names:
  source=root/mapping[group]/(name+('.c' if group=='C_OBJS' else '.cpp'))
  assert source.exists(), source
  sources.add(source)
# These are selected by the upstream platform-specific .mak files, using C implementations.
for name in ['Aes','AesOpt','7zCrc','7zCrcOpt','XzCrc64','XzCrc64Opt','LzFindOpt','LzFindMt','LzmaDec','Sha1','Sha1Opt','Sha256','Sha256Opt','Sort']:
 sources.add(root/'C'/f'{name}.c')
cc=str(tools/'root/usr/bin/i686-w64-mingw32-gcc-win32')
cxx=cc.replace('gcc','g++')
flags=f'-MMD -MP -march=i386 -DZ7_LEGACY_WIN32 -I{tools}/include-aliases -mcrtdll=crtdll -O2 -DZ7_NO_UNICODE -DZ7_EXTERNAL_CODECS -DEXPORT_CODECS -DZ7_WIN32_WINNT_MIN=0x030A -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -DUSE_THREADS_CreateThread -DZ7_LANG -DZ7_DEVICE_FILE'
stamp=build/'flags.txt'
if not stamp.exists() or stamp.read_text()!=flags:
 stamp.write_text(flags)
lines=['.DEFAULT_GOAL := all']
objects=[]
for source in sorted(sources):
 obj=build/(str(source.relative_to(root)).replace('/','_')+'.o'); objects.append(str(obj))
 lines.extend([f'{obj}: {source} {stamp}',f'\t{cc if source.suffix==".c" else cxx} {flags} -c {source} -o {obj}'])
lines.append('all: '+' '.join(objects))
lines.append('-include '+' '.join(str(Path(o).with_suffix('.d')) for o in objects))
(build/'Makefile').write_text('\n'.join(lines)+'\n')
(build/'objects.rsp').write_text('\n'.join(objects))
print(f'{len(sources)} translation units')
