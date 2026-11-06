"""Reject lost or altered GUI resources outside the explicitly changed About dialog."""
import argparse,hashlib,json
from pathlib import Path
import pefile
p=argparse.ArgumentParser(description=__doc__);p.add_argument('reference',type=Path);p.add_argument('current',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
def sha(data):return hashlib.sha256(data).hexdigest()
def resources(path):
 pe=pefile.PE(str(path));out={}
 def visit(node,prefix):
  for entry in node.entries:
   key=prefix+[str(entry.id) if entry.name is None else str(entry.name)]
   if hasattr(entry,'directory'):visit(entry.directory,key)
   else:
    item=entry.data.struct;out['/'.join(key)]=sha(pe.get_data(item.OffsetToData,item.Size))
 visit(pe.DIRECTORY_ENTRY_RESOURCE,[]);return out
old,new=resources(a.reference),resources(a.current)
missing=sorted(set(old)-set(new));added=sorted(set(new)-set(old));changed=sorted(k for k in set(old)&set(new) if old[k]!=new[k]);logo_sha=sha((Path(__file__).resolve().parents[2]/'CPP/7zip/UI/FileManager/7zipLogo.ico').read_bytes())
allowed=lambda k:k.startswith('5/2900/') or (k.startswith('10/100/') and new[k]==logo_sha)
passed=not missing and all(allowed(k) for k in added+changed)
report={'reference_sha256':sha(a.reference.read_bytes()),'current_sha256':sha(a.current.read_bytes()),'reference_resource_count':len(old),'current_resource_count':len(new),'missing':missing,'added':added,'changed':changed,'only_about_changed':passed}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(0 if passed else 1)
