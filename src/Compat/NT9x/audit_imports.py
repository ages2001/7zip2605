"""Compare PE imports against the DLL exports of an unmodified guest OS."""
import argparse
import json
from pathlib import Path
import pefile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary', type=Path)
parser.add_argument('system_directory', type=Path)
args = parser.parse_args()
files = {p.name.lower(): p for p in args.system_directory.iterdir() if p.is_file()}
pe = pefile.PE(str(args.binary))
report = {'binary': str(args.binary), 'subsystem_version': [pe.OPTIONAL_HEADER.MajorSubsystemVersion, pe.OPTIONAL_HEADER.MinorSubsystemVersion], 'missing_dlls': [], 'missing_imports': {}}
for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
    dll = entry.dll.decode('ascii')
    path = files.get(dll.lower())
    if path is None:
        report['missing_dlls'].append(dll)
        continue
    target = pefile.PE(str(path))
    symbols = getattr(getattr(target, 'DIRECTORY_ENTRY_EXPORT', None), 'symbols', [])
    names = {s.name for s in symbols}
    ordinals = {s.ordinal for s in symbols}
    missing = []
    for item in entry.imports:
        if item.name and item.name not in names:
            missing.append(item.name.decode('ascii'))
        elif not item.name and item.ordinal not in ordinals:
            missing.append(f'ordinal:{item.ordinal}')
    if missing:
        report['missing_imports'][dll] = missing
print(json.dumps(report, indent=2))
raise SystemExit(bool(report['missing_dlls'] or report['missing_imports']))
