#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
from pathlib import Path
import subprocess,os,hashlib,json
import argparse
parser=argparse.ArgumentParser(description="Compare native BCJ2 boundary and solid-folder extraction with independent 7-Zip.")
parser.add_argument('--directory',type=Path,required=True)
parser.add_argument('--console',type=Path,required=True)
parser.add_argument('--sevenzip',type=Path,required=True)
parser.add_argument('--seed',type=Path,required=True,help="LARGE.BIN from make_bcj2_fixtures.py --rich")
a=parser.parse_args();root=a.directory.resolve();root.mkdir(parents=True,exist_ok=True)
cache=root/'cache';cache.mkdir(exist_ok=True)
env=dict(os.environ,TEMP=str(cache));host=a.console.resolve();z=str(a.sevenzip.resolve());records=[]
seed=a.seed.read_bytes();assert len(seed)==360003
for index,size in enumerate([1,2,3,4,5,8,15,1023,1024,1025,4095,4096,8193,65535,65536,65537]):
 data=seed[:size];(root/'SMALL.BIN').write_bytes(data)
 for method in ['raw','lzma2']:
  arc=root/('T%02d%s.7Z'%(index,'R' if method=='raw' else 'L'));out='O%02d%s'%(index,method[0].upper())
  cmd=[z,'a','-t7z','-mhc=off','-mmt=off','-m0=BCJ2']
  if method=='lzma2':cmd+=['-m1=LZMA2:d=128k','-m2=LZMA2:d=128k','-m3=LZMA2:d=128k','-mb0:1','-mb0s1:2','-mb0s2:3']
  subprocess.run(cmd+[str(arc),'SMALL.BIN'],cwd=root,check=True,stdout=subprocess.DEVNULL)
  assert subprocess.check_output([z,'x','-so',str(arc)])==data
  for args in [['t',str(arc)],['x',str(arc),out]]:
   p=subprocess.run([str(host)]+args,cwd=root,env=env,capture_output=True);assert p.returncode==0,(args,p.stdout,p.stderr)
  assert (root/out/'SMALL.BIN').read_bytes()==data
  assert not list(cache.iterdir());records.append({'size':size,'method':method,'sha256':hashlib.sha256(data).hexdigest(),'exact':True})
# Multiple folders exercise packed-stream offsets after the first four streams.
# A solid folder exercises file boundaries in the reconstructed BCJ2 output.
for name,data in [('A.BIN',seed[:90001]),('B.BIN',seed[90001:180002]),('C.BIN',seed[180002:]),('EMPTY.BIN',b'')]: (root/name).write_bytes(data)
for solid in ['off','on']:
 arc=root/('MULTI'+solid.upper()+'.7Z');out='M'+solid.upper();cmd=[z,'a','-t7z','-mhc=off','-mmt=off','-ms='+solid,'-m0=BCJ2','-m1=LZMA2:d=128k','-m2=LZMA2:d=128k','-m3=LZMA2:d=128k','-mb0:1','-mb0s1:2','-mb0s2:3',str(arc),'A.BIN','B.BIN','C.BIN','EMPTY.BIN']
 subprocess.run(cmd,cwd=root,check=True,stdout=subprocess.DEVNULL)
 for args in [['t',str(arc)],['x',str(arc),out]]:
  p=subprocess.run([str(host)]+args,cwd=root,env=env,capture_output=True);assert p.returncode==0,(args,p.stdout,p.stderr)
 for name in ['A.BIN','B.BIN','C.BIN','EMPTY.BIN']:assert (root/out/name).read_bytes()==(root/name).read_bytes()
 records.append({'solid':solid,'files':4,'exact':True});assert not list(cache.iterdir())
(root/'results.json').write_text(json.dumps(records,indent=2)+'\n');print('PASS 32 boundary cases and solid/non-solid four-file extraction')
