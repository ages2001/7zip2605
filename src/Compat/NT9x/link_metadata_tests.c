/* Verify explicit link failures and intact archive metadata on legacy FAT.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define entry baseline_entry
#include "guest_tests.c"
#undef entry
void entry(void){char source[MAX_PATH];unsigned i;DWORD tick=GetTickCount(),code;
 copy(directory,"C:\\Z7K00000");for(i=0;i<5;i++)directory[6+i]="0123456789ABCDEF"[(tick>>(16-i*4))&15];
 if(!CreateDirectoryA(directory,NULL)||!SetCurrentDirectoryA(directory))ExitProcess(2);
 report=file("C:\\LINKTEST.TXT",GENERIC_WRITE,CREATE_ALWAYS,FALSE);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 GetModuleFileNameA(NULL,executable,sizeof(executable));i=length(executable);while(i&&executable[i-1]!='\\')i--;copy(executable+i,"7ZZ.EXE");copy(source,executable);copy(source+i,"LINKS.TAR");if(!CopyFileA(source,"LINKS.TAR",TRUE)||!CopyFileA(source,"ORIGINAL.TAR",TRUE))fail("copy link archive",GetLastError());copy(source+i,"DATA.TXT");if(!CopyFileA(source,"DATA.TXT",TRUE))fail("copy expected payload",GetLastError());
 output("TEST DIRECTORY: ");output(directory);output("\r\n");code=run("x -y -oOUT LINKS.TAR","EXTRACT.TXT","EXTRACT.ERR");if(code!=2)fail("unapplied links require error status",code);
 if(!logContains("EXTRACT.ERR","Cannot create hard link"))fail("missing hard-link diagnostic",1);
 if(!logContains("EXTRACT.ERR","Cannot create symbolic link"))fail("missing symbolic-link diagnostic",1);
 compare("OUT\\DATA.TXT","DATA.TXT");compare("LINKS.TAR","ORIGINAL.TAR");output("PASS link metadata remains in unchanged archive; ordinary payload preserved\r\n");success("t LINKS.TAR","TEST.TXT","TEST.ERR");output("PASS ALL\r\n");CloseHandle(report);ExitProcess(0);
}
