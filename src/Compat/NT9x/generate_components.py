"""Build auxiliary GUI, Explorer and SFX components from upstream manifests."""
from pathlib import Path
import os,re
root=Path(__file__).resolve().parents[2]
tools=Path(os.environ.get('NT9X_TOOLS','/workspace/nt9x-tools'))
base=Path(os.environ.get('NT9X_BUILD','/workspace/nt9x-build'))
mapping={'COMMON_OBJS':'CPP/Common','WIN_OBJS':'CPP/Windows','WIN_CTRL_OBJS':'CPP/Windows/Control','7ZIP_COMMON_OBJS':'CPP/7zip/Common','UI_COMMON_OBJS':'CPP/7zip/UI/Common','FM_OBJS':'CPP/7zip/UI/FileManager','AGENT_OBJS':'CPP/7zip/UI/Agent','EXPLORER_OBJS':'CPP/7zip/UI/Explorer','GUI_OBJS':'CPP/7zip/UI/GUI','CONSOLE_OBJS':'CPP/7zip/UI/Console','C_OBJS':'C','AR_OBJS':'CPP/7zip/Archive','AR_COMMON_OBJS':'CPP/7zip/Archive/Common','COMPRESS_OBJS':'CPP/7zip/Compress','CRYPTO_OBJS':'CPP/7zip/Crypto'}
for name,folder in [('7Z','7z'),('CAB','Cab'),('CHM','Chm'),('ISO','Iso'),('NSIS','Nsis'),('RAR','Rar'),('TAR','Tar'),('UDF','Udf'),('WIM','Wim'),('ZIP','Zip')]:mapping[name+'_OBJS']='CPP/7zip/Archive/'+folder
components={
 'gui-command':(['CPP/7zip/Bundles/Format7zF/Arc.mak','CPP/7zip/UI/GUI/makefile'],'CPP/7zip/UI/GUI',''),
 'explorer':(['CPP/7zip/UI/Explorer/makefile'],'CPP/7zip/UI/Explorer',''),
 'sfx-win':(['CPP/7zip/Bundles/SFXWin/makefile'],'CPP/7zip/Bundles/SFXWin','-DZ7_NO_REGISTRY -DZ7_EXTRACT_ONLY -DZ7_NO_READ_FROM_CODER -DZ7_SFX -DZ7_NO_LONG_PATH -DZ7_NO_LARGE_PAGES'),
 'sfx-con':(['CPP/7zip/Bundles/SFXCon/makefile'],'CPP/7zip/Bundles/SFXCon','-DZ7_EXTRACT_ONLY -DZ7_NO_READ_FROM_CODER -DZ7_SFX -DZ7_NO_LONG_PATH -DZ7_NO_LARGE_PAGES'),
}
cc=str(tools/'root/usr/bin/i686-w64-mingw32-gcc-win32');cxx=cc.replace('gcc','g++')
common=(base/'gui-i386/flags.txt').read_text()
for name,(manifests,folder,extra) in components.items():
 build=base/(name+'-i386');build.mkdir(exist_ok=True)
 sources={root/'CPP/7zip/Compress/CodecExports.cpp'} if name=='gui-command' else set()
 for manifest in manifests:
  text=(root/manifest).read_text().replace('\\\n',' ')
  for line in text.splitlines():
   match=re.match(r'([A-Z0-9_]+_OBJS)\s*=\s*(.*)',line)
   if not match:continue
   group,value=match.groups()
   for unit in re.findall(r'\$O\\([\w]+)\.obj',value):
    source=root/(folder if group=='CURRENT_OBJS' else mapping[group])/(unit+('.c' if group=='C_OBJS' else '.cpp'))
    assert source.exists(),source
    sources.add(source)
 # C implementations selected by the upstream architecture include files.
 units=['Sort','Alloc'] if name=='explorer' else ['Aes','AesOpt','7zCrc','7zCrcOpt','LzmaDec','Sha256','Sha256Opt']
 if name=='gui-command':units+=['XzCrc64','XzCrc64Opt','LzFindOpt','LzFindMt','Sha1','Sha1Opt','Sort']
 sources.add(root/'CPP/Windows/FileSystem.cpp')
 for unit in units:sources.add(root/'C'/(unit+'.c'))
 flags=common+(' '+extra if extra else '')
 if name.startswith('sfx-'):flags=flags.replace(' -DZ7_LANG','').replace(' -DZ7_EXTERNAL_CODECS','').replace(' -DEXPORT_CODECS','')
 stamp=build/'flags.txt'
 if not stamp.exists() or stamp.read_text()!=flags:stamp.write_text(flags)
 lines=['.DEFAULT_GOAL := all'];objects=[]
 for source in sorted(sources):
  leaf=str(source.relative_to(root)).replace('/','_')+'.o'
  shared=base/'gui-i386'/leaf
  obj=shared if flags==common and shared.exists() else build/leaf
  objects.append(str(obj))
  if obj!=shared:lines += [f'{obj}: {source} {stamp}',f'\t{cc if source.suffix==".c" else cxx} {flags} -c {source} -o {obj}']
 lines+=['all: '+' '.join(objects),'-include '+' '.join(str(Path(o).with_suffix('.d')) for o in objects)]
 (build/'Makefile').write_text('\n'.join(lines)+'\n');(build/'objects.rsp').write_text('\n'.join(objects)+'\n')
 print(name,len(objects))
