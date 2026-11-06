/* Native NT 3.1/Windows 95 smoke tests. No command-shell dependency.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE report;
static char directory[MAX_PATH], executable[MAX_PATH];
static unsigned length(const char *s) {unsigned n=0;while(s[n])++n;return n;}
static void copy(char *d,const char *s) {while((*d++=*s++)!=0){}}
static void append(char *d,const char *s) {copy(d+length(d),s);}
static void output(const char *s) {DWORD n;WriteFile(report,s,length(s),&n,0);WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,length(s),&n,0);}
static void number(DWORD value) {char s[11];unsigned i;s[0]='0';s[1]='x';for(i=0;i<8;i++)s[2+i]="0123456789ABCDEF"[(value>>(28-4*i))&15];s[10]=0;output(s);}
static void fail(const char *stage,DWORD code) {output("FAIL: ");output(stage);output("; code=");number(code);output("\r\n");CloseHandle(report);ExitProcess(1);}
static HANDLE file(const char *name,DWORD mode,DWORD disposition,BOOL inherit) {
 SECURITY_ATTRIBUTES sa={sizeof(sa),0,inherit};
 return CreateFileA(name,mode,FILE_SHARE_READ,&sa,disposition,FILE_ATTRIBUTE_NORMAL,0);
}
static DWORD run(const char *args,const char *stdout_name,const char *stderr_name) {
 char command[1024];STARTUPINFOA si;PROCESS_INFORMATION pi;DWORD code,error,result;unsigned i;
 HANDLE out=file(stdout_name,GENERIC_WRITE,CREATE_ALWAYS,TRUE),err=file(stderr_name,GENERIC_WRITE,CREATE_ALWAYS,TRUE);
 if(out==INVALID_HANDLE_VALUE||err==INVALID_HANDLE_VALUE)fail("open child log",GetLastError());
 for(i=0;i<sizeof(si);i++)((char*)&si)[i]=0;
 for(i=0;i<sizeof(pi);i++)((char*)&pi)[i]=0;
 si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);si.hStdOutput=out;si.hStdError=err;
 copy(command,"\"");append(command,executable);append(command,"\" ");append(command,args);
 output("RUN: ");output(args);output("\r\n");
 if(!CreateProcessA(executable,command,0,0,TRUE,0,0,directory,&si,&pi)){error=GetLastError();CloseHandle(out);CloseHandle(err);fail("CreateProcess",error);}
 result=WaitForSingleObject(pi.hProcess,180000);
 if(result!=WAIT_OBJECT_0){TerminateProcess(pi.hProcess,99);fail("child timeout or wait failure",result);}
 if(!GetExitCodeProcess(pi.hProcess,&code))fail("GetExitCodeProcess",GetLastError());
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(out);CloseHandle(err);
 output("EXIT: ");number(code);output("\r\n");return code;
}
static void success(const char *args,const char *out,const char *err) {DWORD code=run(args,out,err);if(code)fail(args,code);}
static void compare(const char *a,const char *b) {
 static BYTE x[4096],y[4096];DWORD nx,ny;unsigned i;
 HANDLE fa=file(a,GENERIC_READ,OPEN_EXISTING,FALSE),fb=file(b,GENERIC_READ,OPEN_EXISTING,FALSE);
 if(fa==INVALID_HANDLE_VALUE||fb==INVALID_HANDLE_VALUE)fail("comparison input missing",GetLastError());
 do {
  if(!ReadFile(fa,x,sizeof(x),&nx,0)||!ReadFile(fb,y,sizeof(y),&ny,0))fail("ReadFile",GetLastError());
  if(nx!=ny)fail("round-trip length mismatch",1);
  for(i=0;i<nx;i++)if(x[i]!=y[i])fail("round-trip byte mismatch",1);
 }while(nx);
 CloseHandle(fa);CloseHandle(fb);
}
static BOOL textContains(const char *s,const char *needle){unsigned i,j;for(i=0;s[i];i++){for(j=0;needle[j]&&s[i+j]==needle[j];j++);if(!needle[j])return TRUE;}return FALSE;}
static BOOL logContains(const char *name,const char *needle){static char bytes[16384];DWORD n=0;HANDLE h=file(name,GENERIC_READ,OPEN_EXISTING,FALSE);if(h==INVALID_HANDLE_VALUE)return FALSE;if(!ReadFile(h,bytes,sizeof(bytes)-1,&n,NULL))n=0;CloseHandle(h);bytes[n]=0;return textContains(bytes,needle);}
void entry(void) {
 static const char *formats[]={"7z","zip","gzip","bzip2","xz","tar"};
 static BYTE data[4096];DWORD written,tick=GetTickCount();unsigned i,j;char args[256],archive[16],out[16],err[16];HANDLE input;
 copy(directory,"C:\\Z7T00000");for(i=0;i<5;i++)directory[6+i]="0123456789ABCDEF"[(tick>>(16-i*4))&15];
 if(!CreateDirectoryA(directory,0))ExitProcess(2);
 if(!SetCurrentDirectoryA(directory))ExitProcess(2);
 report=file("RESULT.TXT",GENERIC_WRITE,CREATE_ALWAYS,FALSE);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 if(!GetModuleFileNameA(0,executable,sizeof(executable)))fail("module path",GetLastError());
 i=length(executable);while(i&&executable[i-1]!='\\')--i;copy(executable+i,"7ZZ.EXE");
 output("TEST DIRECTORY: ");output(directory);output("\r\nEXECUTABLE: ");output(executable);output("\r\n");
 input=file("INPUT.BIN",GENERIC_WRITE,CREATE_ALWAYS,FALSE);if(input==INVALID_HANDLE_VALUE)fail("create input",GetLastError());
 tick=0x12345678;
 for(j=0;j<16;j++){
  for(i=0;i<sizeof(data);i++){tick=tick*1664525u+1013904223u;data[i]=(BYTE)(tick>>24);}
  if(!WriteFile(input,data,sizeof(data),&written,0)||written!=sizeof(data))fail("write input",GetLastError());
 }
 CloseHandle(input);
 success("i","INFO.TXT","INFO.ERR");
 if(textContains(GetCommandLineA(),"--plugins")){
  DWORD sum=2166136261UL,n;static BYTE block[4096];char expected[9];HANDLE h=file("INPUT.BIN",GENERIC_READ,OPEN_EXISTING,FALSE);
  if(h==INVALID_HANDLE_VALUE)fail("plugin source",GetLastError());
  while(ReadFile(h,block,sizeof(block),&n,NULL)&&n){unsigned k;for(k=0;k<n;k++)sum=(sum^block[k])*16777619UL;}CloseHandle(h);
  for(i=0;i<8;i++)expected[i]="0123456789ABCDEF"[(sum>>(28-i*4))&15];expected[8]=0;
  if(!logContains("INFO.TXT","NT9xTest"))fail("external hasher is listed",1);
  success("h -scrcNT9xTest INPUT.BIN","PLUGIN.TXT","PLUGIN.ERR");
  if(!logContains("PLUGIN.TXT",expected))fail("external hasher returns independently computed digest",1);
  output("PASS external DLL hashing with independently computed FNV-1a digest\r\n");
 }
 for(i=0;i<6;i++) {
  copy(archive,"ARC0.DAT");archive[3]=(char)('0'+i);copy(out,"OUT0.TXT");out[3]=(char)('0'+i);copy(err,"ERR0.TXT");err[3]=(char)('0'+i);
  copy(args,"a -t");append(args,formats[i]);append(args," -sae -mx1 -mmt2 ");append(args,archive);append(args," INPUT.BIN");success(args,out,err);
  copy(args,"t ");append(args,archive);success(args,"TEST.TXT","TEST.ERR");
  copy(args,"x -so ");append(args,archive);success(args,"DATA.BIN","EXTRACT.ERR");compare("INPUT.BIN","DATA.BIN");
  output("PASS round trip: ");output(formats[i]);output("\r\n");
 }
 success("a -t7z -mx1 -mmt2 -w. WORK.7Z INPUT.BIN","WORK.TXT","WORK.ERR");
 success("x -so WORK.7Z","WORK.BIN","WORKX.ERR");compare("INPUT.BIN","WORK.BIN");
 if(!CopyFileA("INPUT.BIN","SECOND.BIN",TRUE))fail("copy update fixture",GetLastError());
 success("u -t7z -mx1 -mmt2 ARC0.DAT SECOND.BIN","UPDATE.TXT","UPDATE.ERR");
 success("x -so ARC0.DAT SECOND.BIN","UPDATE.BIN","UPDATEX.ERR");compare("INPUT.BIN","UPDATE.BIN");
 output("PASS working-directory staging and existing archive update on 8.3 filesystems\r\n");
 success("a -t7z -mx1 -mmt2 -pTestPassword123 -mhe=on SECRET.7Z INPUT.BIN","CRYPT.TXT","CRYPT.ERR");
 success("t -pTestPassword123 SECRET.7Z","CTEST.TXT","CTEST.ERR");
 success("x -so -pTestPassword123 SECRET.7Z","SECRET.BIN","CEXT.ERR");compare("INPUT.BIN","SECRET.BIN");
 { DWORD code=run("t -pWrongPassword SECRET.7Z","BADPASS.TXT","BADPASS.ERR");
   if(code!=2)fail("wrong password must return archive error",code); }
 output("PASS encryption and wrong-password rejection\r\n");
 { DWORD code=run("t INPUT.BIN","INVALID.TXT","INVALID.ERR");
   if(code!=2)fail("non-archive must return archive error",code); }
 output("PASS malformed input rejection\r\n");
 success("a -t7z -mx1 -mmt2 -v1024b -sae SPLIT INPUT.BIN","SPLIT.TXT","SPLIT.ERR");
 success("x -so SPLIT.001","SPLIT.BIN","SPLITX.ERR");compare("INPUT.BIN","SPLIT.BIN");
 output("PASS split-volume round trip\r\n");
 success("t C:SPLIT.001","DRIVE.TXT","DRIVE.ERR");
 success("x -so C:SPLIT.001","DRIVE.BIN","DRIVEX.ERR");compare("INPUT.BIN","DRIVE.BIN");
 success("t C:ARC?.DAT","WILD.TXT","WILD.ERR");
 output("PASS drive-relative split archive, extraction and wildcard inputs\r\nPASS ALL\r\n");CloseHandle(report);ExitProcess(0);
}
