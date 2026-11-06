"""Historical revision 21-24 packager; not the current combined distribution.

Current installer content contracts and auditing live in Compat/Packaging.
The fixed baseline and revision evidence below intentionally remain unchanged.
"""
from pathlib import Path
import argparse, hashlib, json, zipfile, struct

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('candidate',type=Path,help='Verified candidate with payload/ and SETUP.EXE')
parser.add_argument('output',type=Path,help='New ZIP path outside the source tree')
parser.add_argument('--revision-base',type=Path,help='Previously verified candidate for resource-only GUI and setup revision')
args=parser.parse_args()
source=Path(__file__).resolve().parents[2]
candidate=args.candidate.resolve()
output=args.output.resolve()
if output.exists() or output.is_relative_to(source):
    raise SystemExit('Choose a new output outside the source tree')
payload=candidate/'payload'
expected={p.name.lower():hashlib.sha256(p.read_bytes()).hexdigest() for p in payload.iterdir() if p.is_file()}
required={'7z.exe','7zfm.exe','7zg.exe','7-zip.dll','7z.sfx','7zcon.sfx','uninst.exe','license.txt','readme.txt','history.txt'}
if set(expected)!=required:
    raise SystemExit('Unexpected or incomplete production payload')
results=source/'Compat/NT9x/test-results'
# A resource-only GUI revision reuses prior behavioral evidence only after
# comparing every non-resource PE section and its virtual address. Its new
# installer/uninstaller still require fresh native setup tests on all four OSes.
if args.revision_base:
    current_candidate, current_expected = candidate, expected
    candidate = args.revision_base.resolve()
    expected = {p.name.lower():hashlib.sha256(p.read_bytes()).hexdigest() for p in (candidate/'payload').iterdir() if p.is_file()}
    if set(expected) != required or {n for n in expected if expected[n] != current_expected[n]} != {'7zfm.exe','uninst.exe'}:
        raise SystemExit('Revision changes components outside About and setup')
    def pe_sections(path):
        data=path.read_bytes();pe=struct.unpack_from('<I',data,60)[0]
        if data[pe:pe+4] != b'PE\0\0':raise SystemExit('Invalid PE')
        count=struct.unpack_from('<H',data,pe+6)[0];optional=struct.unpack_from('<H',data,pe+20)[0]
        sections={}
        for i in range(count):
            off=pe+24+optional+40*i;name=data[off:off+8].rstrip(b'\0')
            size,rva,rawsize,raw=struct.unpack_from('<IIII',data,off+8)
            if name == b'.rsrc':continue
            # Relocations are unchanged but follow the enlarged resource.
            sections[name]=(size,0 if name==b'.reloc' else rva,data[raw:raw+rawsize])
        return sections
    if pe_sections(candidate/'payload/7zFM.exe') != pe_sections(current_candidate/'payload/7zFM.exe'):
        raise SystemExit('GUI code/data differs: fresh behavioral regression required')
    digest=hashlib.sha256((current_candidate/'SETUP.EXE').read_bytes()).hexdigest()
    for system in ['nt31','nt351','win95','nt4']:
        directory=results/'revision24'/system
        report=json.loads((directory/'manifest.json').read_text())
        if not report.get('installed_matches_payload') or report['installed_files_sha256'] != current_expected or report['installer_sha256'] != digest:
            raise SystemExit('Revision setup payload mismatch: '+system)
        for name in ['REV24.TXT','SETTEST.TXT','ABOUT24.TXT'] + (['OWN24.TXT'] if system in ['nt31','nt351'] else []):
            text=(directory/name).read_text()
            if not text.strip().endswith('ALL') or any(line.startswith('FAIL') for line in text.splitlines()):
                raise SystemExit('Failed revision regression: '+system+'/'+name)
        for name,sha in report['captured_files_sha256'].items():
            if hashlib.sha256((directory/name).read_bytes()).hexdigest()!=sha:
                raise SystemExit('Modified revision evidence: '+system+'/'+name)
    embedded=json.loads((results/'revision24/installer.json').read_text())
    if not embedded['all_payload_files_embedded'] or embedded['installer_sha256']!=digest or {n.lower():v for n,v in embedded['files_sha256'].items()}!=current_expected:
        raise SystemExit('Revision standalone installer payload mismatch')
baseline=json.loads((results/'nt31/manifest.json').read_text())['installed_files_sha256']
# Reuse the full regression only for byte-identical components. The installer
# correction has its own four-OS setup suite; the association-only File Manager
# revision additionally requires fresh GUI and native-shell evidence on all
# four supplied OS images. Any other binary change still requires a full suite.
if expected != baseline:
    changed = {n for n in expected if expected[n] != baseline.get(n)}
    if set(expected) != set(baseline) or changed not in ({'uninst.exe'}, {'uninst.exe', '7zfm.exe'}):
        raise SystemExit('Application payload changed outside verified revisions: rerun the full regression suite')
    setup_reference = None
    for system in ['nt31','nt351','win95','nt4']:
        directory = results/'setup22'/system
        report = json.loads((directory/'manifest.json').read_text())
        prior = report['installed_files_sha256']
        comparison = dict(expected)
        if '7zfm.exe' in changed:
            comparison['7zfm.exe'] = baseline['7zfm.exe']
        if not report.get('installed_matches_payload') or prior != comparison:
            raise SystemExit('Setup regression does not match unchanged components: '+system)
        if setup_reference is None:
            setup_reference = report['installer_sha256']
        if report['installer_sha256'] != setup_reference:
            raise SystemExit('Inconsistent setup evidence: '+system)
        for name in [report['runner_log'], 'SETTEST.TXT']:
            text = (directory/name).read_text()
            if not text.strip().endswith('ALL') or any(line.startswith('FAIL') for line in text.splitlines()):
                raise SystemExit('Failed setup regression: '+system+'/'+name)
        for name,digest in report['captured_files_sha256'].items():
            if hashlib.sha256((directory/name).read_bytes()).hexdigest() != digest:
                raise SystemExit('Modified setup evidence: '+system+'/'+name)
    setup_digest = hashlib.sha256((candidate/'SETUP.EXE').read_bytes()).hexdigest()
    if '7zfm.exe' not in changed and setup_digest != setup_reference:
        raise SystemExit('Installer differs from setup-only regression')
    if '7zfm.exe' in changed:
        for system in ['nt31','nt351','win95','nt4']:
            directory = results/'final23'/system
            report = json.loads((directory/'manifest.json').read_text())
            if not report.get('installed_matches_payload') or report['installed_files_sha256'] != expected or report['installer_sha256'] != setup_digest:
                raise SystemExit('Final GUI payload mismatch: '+system)
            for name in ['ASSTEST.TXT','GUIADV.TXT','GUIHELP.TXT']:
                text = (directory/name).read_text()
                if not text.strip().endswith('ALL') or any(line.startswith('FAIL') for line in text.splitlines()):
                    raise SystemExit('Failed final GUI regression: '+system+'/'+name)
            for name,digest in report['captured_files_sha256'].items():
                if hashlib.sha256((directory/name).read_bytes()).hexdigest() != digest:
                    raise SystemExit('Modified final GUI evidence: '+system+'/'+name)
        embedded = json.loads((results/'final23/installer.json').read_text())
        if not embedded['all_payload_files_embedded'] or embedded['installer_sha256'] != setup_digest or {n.lower():v for n,v in embedded['files_sha256'].items()} != expected:
            raise SystemExit('Standalone installer payload mismatch')
        network = json.loads((results/'network-nt31/manifest.json').read_text())
        if network['cli_sha256'] != expected['7z.exe'] or network['gui_sha256'] != expected['7zfm.exe']:
            raise SystemExit('Live network test does not match final executables')
        for name,digest in network['captured_files_sha256'].items():
            data = (results/'network-nt31'/name).read_bytes()
            if hashlib.sha256(data).hexdigest() != digest:
                raise SystemExit('Modified network evidence: '+name)
        for name in ['NETSMOKE.TXT','GUINET.TXT']:
            text = (results/'network-nt31'/name).read_text()
            if not text.strip().endswith('ALL') or any(line.startswith('FAIL') for line in text.splitlines()):
                raise SystemExit('Failed live network regression: '+name)
for system in ['nt31','win95','nt4','bochs']:
    directory=results/system
    report=json.loads((directory/'manifest.json').read_text())
    if not report.get('installed_matches_payload') or report['installed_files_sha256']!=baseline:
        raise SystemExit('Installed payload verification failed: '+system)
    for name in ['REGRESS.TXT','GUIHELP.TXT','LINKTEST.TXT']:
        text=(directory/name).read_text()
        if not text.strip().endswith('PASS ALL') or any(line.startswith('FAIL') for line in text.splitlines()):
            raise SystemExit('Unfinished or failed test: '+system+'/'+name)
    for name,digest in report['captured_files_sha256'].items():
        if hashlib.sha256((directory/name).read_bytes()).hexdigest()!=digest:
            raise SystemExit('Modified test evidence: '+system+'/'+name)
if 'PENDING' in (source/'Compat/NT9x/TESTING.md').read_text():
    raise SystemExit('Verification documentation still has pending work')
if args.revision_base:
    candidate, expected = current_candidate, current_expected
files={}
for p in payload.iterdir():files['bin/'+p.name]=p.read_bytes()
files['bin/SETUP.EXE']=(candidate/'SETUP.EXE').read_bytes()
# Include all source, support scripts, documentation and evidence. Exclude Git
# administration and Python caches; no build directory lives in this tree.
for p in source.rglob('*'):
    rel=p.relative_to(source)
    if any(part in ['.git','__pycache__'] for part in rel.parts):continue
    if not p.is_file():continue
    if p.is_symlink():raise SystemExit('Unexpected source symlink: '+str(rel))
    if p.suffix.lower() in ['.vhd','.vhdx','.qcow2','.img','.raw','.pyc','.exe','.dll','.o','.a']:
        raise SystemExit('Unexpected binary in source: '+str(rel))
    files['src/'+rel.as_posix()]=p.read_bytes()
files['README.txt']=b'''7-Zip 26.05 NT/9x i386 compatibility distribution\r\n\r\nThis is an unofficial port.\r\n\r\nRun bin\\SETUP.EXE to install, or use the programs in bin directly.\r\nNT 3.x: C:\\7-Zip, Program Manager shortcuts.\r\nWindows 95 / NT4: Program Files, Desktop and Programs shortcuts.\r\n\r\nbin contains the tested executables, installer and license notices.\r\nsrc contains the complete 7-Zip application source, compatibility modules,\r\nbuild scripts and test evidence. See src\\Compat\\NT9x\\BUILD.md and TESTING.md.\r\n\r\nOn an NT3.1 FAT volume, extract only bin: upstream source filenames require\r\nlong-name support and the source is intended to be rebuilt on Linux.\r\n\r\nSHA256SUMS.txt records every packaged file except itself.\r\n'''
files['SHA256SUMS.txt']=''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in sorted(files.items())).encode('ascii')
output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
    for name,data in sorted(files.items()):archive.writestr(name,data)
with zipfile.ZipFile(output) as archive:
    bad=archive.testzip()
    if bad:raise RuntimeError('ZIP CRC failure: '+bad)
    for name,line in [(line.split('  ',1)[1],line) for line in archive.read('SHA256SUMS.txt').decode().splitlines()]:
        if hashlib.sha256(archive.read(name)).hexdigest()!=line.split('  ',1)[0]:
            raise RuntimeError('ZIP SHA-256 failure: '+name)
print(output)
print('Files:',len(files),'Bytes:',output.stat().st_size)
print('SHA256:',hashlib.sha256(output.read_bytes()).hexdigest())
