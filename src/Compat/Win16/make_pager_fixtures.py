#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,random,struct,zlib,json
import argparse
p=argparse.ArgumentParser(description='Independent long-distance LZMA/LZMA2 fixtures for the Win16 pager')
p.add_argument('--directory',type=Path,required=True);p.add_argument('--sevenzip',type=Path,required=True);a=p.parse_args()
b=a.directory.resolve();b.mkdir(parents=True,exist_ok=True)
r=random.Random(53);seed=bytes(r.randrange(256) for _ in range(90000));(b/'LARGE.BIN').write_bytes(seed*4)
e=os.environ.copy();exe=str(a.sevenzip.resolve())
for method,stem in [('LZMA','LZ'),('LZMA2','L2')]:
 path=b/(stem+'128.7Z');subprocess.run([exe,'a','-t7z','-m0='+method+':d=128k','-mhc=off','-y',str(path),'LARGE.BIN'],cwd=b,env=e,check=True,stdout=subprocess.DEVNULL)
 # Increase only the decoder's dictionary capacity. The same independently
 # encoded distances remain valid; recompute both format checksums.
 data=bytearray(path.read_bytes());off=32+struct.unpack_from('<Q',data,12)[0];size=struct.unpack_from('<Q',data,20)[0]
 header=data[off:off+size]
 if method=='LZMA':
  marker=bytes.fromhex('2303010105');i=header.index(marker)+len(marker);old=header[i+1:i+5];header[i+1:i+5]=struct.pack('<I',32*1024*1024)
 else:
  marker=bytes.fromhex('212101');i=header.index(marker)+len(marker);old=header[i:i+1];header[i]=26 # 32 MiB
 data[off:off+size]=header;struct.pack_into('<I',data,28,zlib.crc32(header));struct.pack_into('<I',data,8,zlib.crc32(data[12:32]));(b/(stem+'32M.7Z')).write_bytes(data)
 print(path.name,path.stat().st_size,'first packed byte',hex(data[32]),'old property',old.hex())
 for suffix in ['128','32M']:
  a=b/(stem+suffix+'.7Z');subprocess.run([exe,'t',str(a)],env=e,check=True,stdout=subprocess.DEVNULL)
  out=subprocess.check_output([exe,'l','-slt',str(a)],env=e,text=True);(b/(stem+suffix+'.slt')).write_text(out)
 print(out[out.index('Method ='):out.index('Method =')+40])
(b/'expected.json').write_text(json.dumps({'size':360000,'crc32':'%08x'%zlib.crc32(seed*4),'distance':90000},indent=2)+'\n')

d=bytearray((b/'L232M.7Z').read_bytes());d[32+10017]^=0x80;(b/'L2BAD.7Z').write_bytes(d)
assert (b/'L2128.7Z').read_bytes()[32]==1, 'Fixture must exercise an uncompressed LZMA2 chunk'
