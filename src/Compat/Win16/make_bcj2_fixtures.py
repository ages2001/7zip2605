#!/usr/bin/env python3
"""Independent four-stream BCJ2 archives for native decoder regression."""
from pathlib import Path
import argparse, hashlib, json, random, subprocess, zlib
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--directory',type=Path,required=True)
p.add_argument('--sevenzip',type=Path,required=True)
p.add_argument('--rich',action='store_true')
a=p.parse_args();root=a.directory.resolve();root.mkdir(parents=True,exist_ok=True)
r=random.Random(53);data=bytes(r.randrange(256) for _ in range(90000))*4+b'\xe8\x01\x02'
if a.rich:
 data=bytearray(data)
 for offset in range(0,360000,32):
  for start,opcode in [(offset,b'\xe8'),(offset+8,b'\xe9'),(offset+16,b'\x0f\x85')]:
   end=start+len(opcode)+4;target=(start+3333)%360000
   data[start:end]=opcode+((target-end)&0xffffffff).to_bytes(4,'little')
 data=bytes(data)
(root/'LARGE.BIN').write_bytes(data)
cases={'BRAW':['BCJ2'],'BLZMA':['BCJ2']+['LZMA:d=128k']*3,
       'BLZMA2':['BCJ2']+['LZMA2:d=128k']*3,'BDFLT':['BCJ2']+['Deflate']*3}
manifest={'size':len(data),'crc32':'%08x'%zlib.crc32(data),'sha256':hashlib.sha256(data).hexdigest(),'archives':{}}
for name,methods in cases.items():
 archive=root/(name+'.7Z')
 if archive.exists():raise FileExistsError(archive)
 cmd=[str(a.sevenzip.resolve()),'a','-t7z','-mhc=off','-mmt=off']+['-m%d=%s'%(i,m) for i,m in enumerate(methods)]
 if len(methods)>1:cmd+=['-mb0:1','-mb0s1:2','-mb0s2:3']
 subprocess.run(cmd+[str(archive),'LARGE.BIN'],cwd=root,check=True,stdout=subprocess.DEVNULL)
 assert subprocess.check_output([str(a.sevenzip.resolve()),'x','-so',str(archive)])==data
 manifest['archives'][archive.name]={'methods':methods,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'independent_decode':True}
bad=bytearray((root/'BRAW.7Z').read_bytes());offset=bad.index(0x40,132,10032);bad[offset]^=1
(root/'BBAD.7Z').write_bytes(bad)
check=subprocess.run([str(a.sevenzip.resolve()),'t',str(root/'BBAD.7Z')],capture_output=True)
assert check.returncode and b'CRC Failed' in check.stderr+check.stdout
manifest['damaged_byte_offset']=offset
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
