/* Orchestrate native guest tests in disposable overlays. Never ship in bin/.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void *memset(void *p,int c,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=c;return p;}
static HANDLE report;static char directory[MAX_PATH];static unsigned failures;
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);}
static void run(const char *name,const char *args){char exe[MAX_PATH],command[3*MAX_PATH],message[100];STARTUPINFOA si;PROCESS_INFORMATION pi;DWORD code=0xffffffff;wsprintfA(exe,"%s%s",directory,name);wsprintfA(command,"\"%s\" %s",exe,args);line(command);ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);if(CreateProcessA(exe,command,NULL,NULL,FALSE,0,NULL,directory,&si,&pi)){CloseHandle(pi.hThread);if(WaitForSingleObject(pi.hProcess,900000)==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);}else code=GetLastError();wsprintfA(message,"%s exit=%lu",code?"FAIL":"PASS",code);line(message);if(code)failures++;}
static BOOL CALLBACK closeOld(HWND w,LPARAM unused){char name[80];GetClassNameA(w,name,sizeof(name));if(!lstrcmpA(name,"7-Zip::FM"))PostMessageA(w,WM_CLOSE,0,0);return TRUE;}
void entry(void){DWORD n,start=GetTickCount();char elapsed[80];
 report=CreateFileA("C:\\VISUAL25.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 GetModuleFileNameA(NULL,directory,MAX_PATH);n=lstrlenA(directory);while(n&&directory[n-1]!='\\')n--;directory[n]=0;
 run("CTTEST.EXE","");run("LVTEST.EXE","");run("ICONTEST.EXE","");run("ABOUT24.EXE","");run("GUIROUND.EXE","--advanced");run("GUIHELP.EXE","");run("ASSTEST.EXE","");
 wsprintfA(elapsed,"Elapsed milliseconds: %lu",GetTickCount()-start);line(elapsed);
 line(failures?"FAIL ALL":"PASS ALL");CloseHandle(report);ExitProcess(failures?1:0);
}
