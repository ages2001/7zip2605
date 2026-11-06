/* Short live SMB test against an isolated host-only test share.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnetwk.h>
static HANDLE report,cliOutput;
void *memset(void *p,int c,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=(BYTE)c;return p;}
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);}
static void check(BOOL ok,const char *s){char b[256];wsprintfA(b,"%s: %s (last error %lu)",ok?"PASS":"FAIL",s,ok?0:GetLastError());line(b);if(!ok){CloseHandle(report);ExitProcess(1);}}
static void run(const char *exe,const char *args){char command[1024];STARTUPINFOA si;PROCESS_INFORMATION pi;DWORD code=~0UL;ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=si.hStdError=cliOutput;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);wsprintfA(command,"\"%s\" %s",exe,args);line(command);check(CreateProcessA(exe,command,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi),"start installed CLI");CloseHandle(pi.hThread);if(WaitForSingleObject(pi.hProcess,60000)!=WAIT_OBJECT_0){TerminateProcess(pi.hProcess,98);CloseHandle(pi.hProcess);SetLastError(ERROR_TIMEOUT);check(FALSE,"CLI timeout");}GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);SetLastError(code);check(code==0,"CLI operation succeeds");}
void entry(void){char unc[MAX_PATH],mapped[MAX_PATH],source[MAX_PATH],archive[MAX_PATH],output[MAX_PATH],exe[MAX_PATH],args[900],remote[MAX_PATH];static BYTE data[4096],actual[4096],resources[8192];DWORD n,i,count,size,error,start=GetTickCount();HANDLE f,enumeration;BOOL found=FALSE;DWORD version=GetVersion();
 report=CreateFileA("C:\\NETSMOKE.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 {SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};cliOutput=CreateFileA("C:\\NETCLI.TXT",GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);check(cliOutput!=INVALID_HANDLE_VALUE,"open CLI diagnostic log");}
 error=WNetAddConnectionA("\\\\Z7HOST\\Z7TEST","","S:");SetLastError(error);check(error==NO_ERROR,"connect mapped drive to live SMB share");
 size=sizeof(remote);error=WNetGetConnectionA("S:",remote,&size);SetLastError(error);check(error==NO_ERROR&&!lstrcmpiA(remote,"\\\\Z7HOST\\Z7TEST"),"MPR reports mapped UNC target");
 error=WNetOpenEnumA(RESOURCE_CONNECTED,RESOURCETYPE_DISK,0,NULL,&enumeration);SetLastError(error);check(error==NO_ERROR,"open connected-resource enumeration");
 for(;;){NETRESOURCEA *r=(NETRESOURCEA*)resources;size=sizeof(resources);count=0xffffffff;error=WNetEnumResourceA(enumeration,&count,resources,&size);if(error==ERROR_NO_MORE_ITEMS)break;SetLastError(error);check(error==NO_ERROR,"enumerate live connected resources");if(!count)break;for(i=0;i<count;i++)if(r[i].lpRemoteName&&!lstrcmpiA(r[i].lpRemoteName,"\\\\Z7HOST\\Z7TEST"))found=TRUE;}
 WNetCloseEnum(enumeration);check(found,"live share is present in MPR enumeration");
 wsprintfA(unc,"\\\\Z7HOST\\Z7TEST\\N%07lX",start&0xfffffff);wsprintfA(mapped,"S:\\N%07lX",start&0xfffffff);check(CreateDirectoryA(unc,NULL),"create private UNC workspace");line(unc);
 wsprintfA(source,"%s\\INPUT.BIN",unc);wsprintfA(archive,"%s\\NET.7Z",unc);wsprintfA(output,"%s\\OUT",mapped);
 for(i=0;i<sizeof(data);i++)data[i]=(BYTE)(i*37+i/13);
 f=CreateFileA(source,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);check(f!=INVALID_HANDLE_VALUE,"create source on SMB share");check(WriteFile(f,data,sizeof(data),&n,NULL)&&n==sizeof(data),"write exact network source bytes");CloseHandle(f);
 lstrcpyA(exe,!(version&0x80000000UL)&&(version&255)==3?"C:\\7-Zip\\7z.exe":"C:\\Program Files\\7-Zip\\7z.exe");
 wsprintfA(args,"a -t7z -mx1 \"%s\" \"%s\"",archive,source);run(exe,args);wsprintfA(args,"t \"%s\"",archive);run(exe,args);wsprintfA(args,"x -y -o\"%s\" \"%s\"",output,archive);run(exe,args);
 wsprintfA(source,"%s\\INPUT.BIN",output);f=CreateFileA(source,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);check(f!=INVALID_HANDLE_VALUE,"read extracted file through mapped drive");check(ReadFile(f,actual,sizeof(actual),&n,NULL)&&n==sizeof(actual),"read complete extracted data");check(GetFileSize(f,NULL)==sizeof(data),"extracted file has exact length");CloseHandle(f);for(i=0;i<sizeof(data);i++)if(actual[i]!=data[i])check(FALSE,"network round-trip byte mismatch");check(TRUE,"all network round-trip bytes match");
 error=WNetCancelConnectionA("S:",FALSE);SetLastError(error);check(error==NO_ERROR,"disconnect owned mapping");wsprintfA(args,"Elapsed milliseconds: %lu",GetTickCount()-start);line(args);line("PASS ALL");CloseHandle(cliOutput);CloseHandle(report);ExitProcess(0);
}
