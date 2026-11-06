/* Test the unmodified user-supplied Modern7z package in disposable guests.
   Record loader errors separately from actual codec operations. */
#include <windows.h>
void *memset(void *p,int v,unsigned n){unsigned char *q=p;while(n--)*q++=(unsigned char)v;return p;}
void *memcpy(void *p,const void *s,unsigned n){unsigned char *q=p;const unsigned char *r=s;while(n--)*q++=*r++;return p;}
static HANDLE logFile;
static void record(const char *text){DWORD n;WriteFile(logFile,text,lstrlenA(text),&n,0);WriteFile(logFile,"\r\n",2,&n,0);FlushFileBuffers(logFile);}
static void run(const char *exe,const char *args,const char *output){
 STARTUPINFOA si;PROCESS_INFORMATION pi;SECURITY_ATTRIBUTES sa;HANDLE out;char cmd[1024],line[1100];DWORD code,wait;
 memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));sa.nLength=sizeof(sa);sa.lpSecurityDescriptor=0;sa.bInheritHandle=TRUE;
 out=CreateFileA(output,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,0,0);
 if(out==INVALID_HANDLE_VALUE){record("FAIL output creation");return;}
 si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;si.hStdOutput=si.hStdError=out;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
 wsprintfA(cmd,"\"%s\" %s",exe,args);
 if(CreateProcessA(exe,cmd,0,0,TRUE,0,0,"C:\\MODTEST",&si,&pi)){
  wait=WaitForSingleObject(pi.hProcess,60000);
  if(wait!=WAIT_OBJECT_0){TerminateProcess(pi.hProcess,124);WaitForSingleObject(pi.hProcess,5000);}
  code=0xffffffff;GetExitCodeProcess(pi.hProcess,&code);wsprintfA(line,"COMMAND %s; wait=%lu exit=%lu",args,wait,code);record(line);
  CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 }else{wsprintfA(line,"CREATE ERROR %lu: %s",GetLastError(),args);record(line);}
 FlushFileBuffers(out);CloseHandle(out);
}
void entry(void){
 static const char *names[]={"Modern7z.32.dll","brotli\\brotlicommon.dll","brotli\\brotlidec.dll","brotli\\brotlienc.dll","lz4\\liblz4.32.dll","lz5\\liblz5.32.dll","lizard\\liblizard.32.dll","zstd\\libzstd.32.dll","zstd\\libzstd.xp.32.dll","fast-lzma2\\fast-lzma2.32.dll"};
 static const char *methods[]={"ZSTD","BROTLI","LZ4","LZ5","LIZARD","FLZMA2"};
 char base[MAX_PATH],path[MAX_PATH],line[600],exe[MAX_PATH],args[256],out[MAX_PATH];unsigned i;HMODULE h;DWORD version=GetVersion(),err,n;HANDLE data;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX|SEM_NOGPFAULTERRORBOX);
 logFile=CreateFileA("C:\\MODERN.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 wsprintfA(line,"OS version raw=%lu; unmodified Modern7z x86 package",version);record(line);
 lstrcpyA(base,(!(version&0x80000000UL)&&(version&255)==3)?"C:\\7-Zip":"C:\\Program Files\\7-Zip");
 wsprintfA(exe,"%s\\7z.exe",base);CreateDirectoryA("C:\\MODTEST",0);SetCurrentDirectoryA("C:\\MODTEST");
 data=CreateFileA("C:\\MODTEST\\DATA.TXT",GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);for(i=0;i<128;i++)WriteFile(data,"Modern7z real codec round trip.\r\n",32,&n,0);CloseHandle(data);
 for(i=0;i<sizeof(names)/sizeof(names[0]);i++){
  wsprintfA(path,"%s\\Codecs\\%s",base,names[i]);SetLastError(0);h=LoadLibraryExA(path,0,LOAD_WITH_ALTERED_SEARCH_PATH);err=h?0:GetLastError();wsprintfA(line,"LOAD %s: %s error=%lu",names[i],h?"OK":"FAIL",err);record(line);if(h)FreeLibrary(h);
 }
 run(exe,"i","C:\\MODINFO.TXT");
 for(i=0;i<sizeof(methods)/sizeof(methods[0]);i++){
  wsprintfA(args,"a -t7z -m0=%s -mx=1 -mmt=1 -y C:\\MODTEST\\%s.7z C:\\MODTEST\\DATA.TXT",methods[i],methods[i]);wsprintfA(out,"C:\\MODTEST\\%s.LOG",methods[i]);run(exe,args,out);
  wsprintfA(args,"t C:\\MODTEST\\%s.7z",methods[i]);wsprintfA(out,"C:\\MODTEST\\%s.TST",methods[i]);run(exe,args,out);
 }
 record("TEST COMPLETE (inspect results; completion is not a pass)");CloseHandle(logFile);ExitProcess(0);
}
