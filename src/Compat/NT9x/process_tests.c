/* Native process snapshot and file-size regression tests. No CRT.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
void *memset(void *p,int v,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=(BYTE)v;return p;}
void *memcpy(void *d,const void *s,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)d)[i]=((const BYTE*)s)[i];return d;}
static HANDLE report;
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);}
static void hex(DWORD value){char s[9];unsigned i;for(i=0;i<8;i++)s[i]="0123456789ABCDEF"[(value>>(28-4*i))&15];s[8]=0;line(s);}
static void check(BOOL b,const char *s){if(!b){line("FAIL");line(s);CloseHandle(report);ExitProcess(1);}}
void entry(void){
 char *command=GetCommandLineA(),name[32]="Z7PS00000000",path[MAX_PATH],args[2*MAX_PATH];DWORD self=GetCurrentProcessId(),i,error,size,high,written,code;HANDLE event,snapshot,file;PROCESSENTRY32 entry;STARTUPINFOA startup;PROCESS_INFORMATION child;BOOL found_self=FALSE,found_child=FALSE;
 for(i=0;command[i];i++)if(command[i]=='!'){event=OpenEventA(SYNCHRONIZE,FALSE,command+i+1);if(!event)ExitProcess(3);code=WaitForSingleObject(event,30000);CloseHandle(event);ExitProcess(code==WAIT_OBJECT_0?0:4);}
 report=CreateFileA("C:\\PROCTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 for(i=0;i<8;i++)name[4+i]="0123456789ABCDEF"[(self>>(28-i*4))&15];
 event=CreateEventA(NULL,TRUE,FALSE,name);check(event!=NULL,"create child synchronization event");
 GetModuleFileNameA(NULL,path,sizeof(path));lstrcpyA(args,"\"");lstrcatA(args,path);lstrcatA(args,"\" !");lstrcatA(args,name);
 ZeroMemory(&startup,sizeof(startup));startup.cb=sizeof(startup);ZeroMemory(&child,sizeof(child));
 check(CreateProcessA(path,args,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child),"create child process");
 snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);check(snapshot!=INVALID_HANDLE_VALUE,"create process snapshot");
 ZeroMemory(&entry,sizeof(entry));entry.dwSize=sizeof(entry);check(Process32First(snapshot,&entry),"first process");
 do {check(entry.cntThreads>0,"nonzero process thread count");if(entry.th32ProcessID==self){found_self=TRUE;check(entry.szExeFile[0]!=0,"self process name");}
  if(entry.th32ProcessID==child.dwProcessId){found_child=TRUE;hex(self);hex(entry.th32ParentProcessID);line(entry.szExeFile);check(entry.th32ParentProcessID==self&&entry.szExeFile[0],"child parent and process name");}
 }while(Process32Next(snapshot,&entry));
 check(GetLastError()==ERROR_NO_MORE_FILES&&found_self&&found_child,"complete snapshot and expected processes");
 entry.dwSize=sizeof(entry);check(Process32First(snapshot,&entry),"restart enumeration");
 entry.dwSize=0;check(!Process32Next(snapshot,&entry),"reject invalid entry size");check(CloseHandle(snapshot),"close snapshot kernel handle");
 check(SetEvent(event),"release child");check(WaitForSingleObject(child.hProcess,30000)==WAIT_OBJECT_0,"wait for child");check(GetExitCodeProcess(child.hProcess,&code)&&code==0,"child synchronization succeeded");CloseHandle(child.hThread);CloseHandle(child.hProcess);CloseHandle(event);
 line("PASS process enumeration, parent identity, restart, invalid size, handle cleanup");
 if(!(GetVersion()&0x80000000UL)){
  file=CreateFileW(L"C:\\PS-SIZE.TMP",GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);check(file!=INVALID_HANDLE_VALUE,"create file-size test input");
  check(WriteFile(file,"123456789",9,&written,NULL)&&written==9,"write file-size test input");CloseHandle(file);
  high=123;SetLastError(123);size=GetCompressedFileSizeW(L"C:\\PS-SIZE.TMP",&high);error=GetLastError();
  check(size==9&&high==0&&error==0,"uncompressed physical file size");check(DeleteFileW(L"C:\\PS-SIZE.TMP"),"delete file-size test input");
  SetLastError(0);size=GetCompressedFileSizeW(L"C:\\PS-SIZE.TMP",&high);check(size==INVALID_FILE_SIZE&&GetLastError()==ERROR_FILE_NOT_FOUND,"missing-file error");
  line("PASS uncompressed file size and missing-file error");
 }else line("SKIP Unicode compressed-file-size API on Windows 95");
 line("PASS ALL");CloseHandle(report);ExitProcess(0);
}
