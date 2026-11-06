"""Remove MinGW's callback-only TLS directory, using legacy_dll_entry.c instead.

Fail closed if the image contains real static TLS data. DLL thread callbacks
remain necessary for the C++ runtime and are dispatched explicitly, not dropped.
"""
from pathlib import Path
import sys
import pefile
path=Path(sys.argv[1]);pe=pefile.PE(str(path))
if not (pe.FILE_HEADER.Characteristics & 0x2000):raise SystemExit('Not a DLL')
tls=pe.DIRECTORY_ENTRY_TLS.struct
start=tls.StartAddressOfRawData-pe.OPTIONAL_HEADER.ImageBase
size=tls.EndAddressOfRawData-tls.StartAddressOfRawData
if size!=4 or tls.SizeOfZeroFill!=0 or pe.get_data(start,size)!=b'\0'*4:
 raise SystemExit('Unexpected static TLS storage; refusing to remove its directory')
pe.OPTIONAL_HEADER.DATA_DIRECTORY[9].VirtualAddress=0
pe.OPTIONAL_HEADER.DATA_DIRECTORY[9].Size=0
pe.OPTIONAL_HEADER.CheckSum=0
pe.OPTIONAL_HEADER.CheckSum=pe.generate_checksum()
pe.write(str(path))
