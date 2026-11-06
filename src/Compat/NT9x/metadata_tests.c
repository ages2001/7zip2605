/* FAT extraction tests for security descriptors and alternate data streams.
   The source WIM must remain byte-for-byte intact after unsupported operations.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define entry baseline_entry
#include "guest_tests.c"
#undef entry
static BOOL contains(const char *path,const char *text) {
  static char buffer[32768];DWORD n;unsigned i,j;HANDLE f=file(path,GENERIC_READ,OPEN_EXISTING,FALSE);
  if(f==INVALID_HANDLE_VALUE)fail("missing diagnostic log",GetLastError());
  if(!ReadFile(f,buffer,sizeof(buffer)-1,&n,NULL))fail("read diagnostic log",GetLastError());CloseHandle(f);buffer[n]=0;
  for(i=0;i<n;i++){for(j=0;text[j]&&i+j<n&&buffer[i+j]==text[j];j++);if(!text[j])return TRUE;}return FALSE;
}
void entry(void) {
  char source[MAX_PATH];unsigned i;DWORD tick=GetTickCount(),code;
  copy(directory,"C:\\Z7M00000");for(i=0;i<5;i++)directory[6+i]="0123456789ABCDEF"[(tick>>(16-i*4))&15];
  if(!CreateDirectoryA(directory,NULL)||!SetCurrentDirectoryA(directory))ExitProcess(2);
  report=file("RESULT.TXT",GENERIC_WRITE,CREATE_ALWAYS,FALSE);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
  if(!GetModuleFileNameA(NULL,executable,sizeof(executable)))fail("module path",GetLastError());
  i=length(executable);while(i&&executable[i-1]!='\\')--i;copy(executable+i,"7ZZ.EXE");
  copy(source,executable);copy(source+i,"META.WIM");if(!CopyFileA(source,"META.WIM",TRUE))fail("copy WIM",GetLastError());
  if(!CopyFileA(source,"ORIGINAL.WIM",TRUE))fail("copy original WIM",GetLastError());
  copy(source+i,"DATA.TXT");if(!CopyFileA(source,"DATA.TXT",TRUE))fail("copy payload",GetLastError());
  output("TEST DIRECTORY: ");output(directory);output("\r\n");
  code=run("x -sni -y -oOUT META.WIM","EXTRACT.TXT","EXTRACT.ERR");
  if(code!=2)fail("unapplied metadata must return error status",code);
  if(!contains("EXTRACT.ERR","Cannot apply NT security information"))fail("missing security diagnostic",1);
  if(!contains("EXTRACT.ERR","Cannot apply alternate data stream"))fail("missing stream diagnostic",1);
  if(!contains("EXTRACT.ERR","original archive retains"))fail("missing preservation diagnostic",1);
  compare("OUT\\F","DATA.TXT");compare("META.WIM","ORIGINAL.WIM");
  output("PASS unsupported metadata is reported; payload and original archive preserved\r\n");
  success("x -snr -y -oFLAT META.WIM","FLAT.TXT","FLAT.ERR");
  compare("FLAT\\F","DATA.TXT");compare("FLAT\\F_S","DATA.TXT");compare("META.WIM","ORIGINAL.WIM");
  output("PASS explicit stream-to-file extraction preserves both payloads\r\n");
  success("t META.WIM","TEST.TXT","TEST.ERR");
  output("PASS ALL\r\n");CloseHandle(report);ExitProcess(0);
}
