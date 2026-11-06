/* Unicode entry points used by the upstream installer on Windows 95.
   NT retains native Unicode APIs. Conversion rejects unrepresentable paths.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define BIND(name,bytes) __typeof__(&name) setup_import_##name __asm__("__imp__" #name "@" #bytes)=setup_##name
#define NATIVE(dll,name) ((__typeof__(&name))GetProcAddress(GetModuleHandleA(dll),#name))
static BOOL nt(void){return !(GetVersion()&0x80000000UL);}
static void release(void *p){if(p&&(UINT_PTR)p>65535)HeapFree(GetProcessHeap(),0,p);}
static char *ansi(const WCHAR *s){BOOL used=FALSE;int n;char *p;if(!s||IS_INTRESOURCE(s))return (char*)s;n=WideCharToMultiByte(CP_ACP,0,s,-1,NULL,0,NULL,&used);if(!n||used){SetLastError(ERROR_NO_UNICODE_TRANSLATION);return NULL;}p=HeapAlloc(GetProcessHeap(),0,n);if(!p){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}if(!WideCharToMultiByte(CP_ACP,0,s,-1,p,n,NULL,&used)||used){release(p);SetLastError(ERROR_NO_UNICODE_TRANSLATION);return NULL;}return p;}
static DWORD wideOutput(const char *s,WCHAR *out,DWORD cap,BOOL requiredIncludesNull){int need=MultiByteToWideChar(CP_ACP,0,s,-1,NULL,0);if(!need)return 0;if(!out||cap<(DWORD)need){SetLastError(ERROR_INSUFFICIENT_BUFFER);return requiredIncludesNull?need:0;}MultiByteToWideChar(CP_ACP,0,s,-1,out,cap);return need-1;}
#define KERNEL_ONE(name,ATYPE,FAIL) static ATYPE WINAPI setup_##name##W(LPCWSTR value){__typeof__(&name##W) fn=NATIVE("kernel32.dll",name##W);char *a;ATYPE r;if(nt())return fn(value);a=ansi(value);if(value&&!a)return FAIL;r=name##A(a);release(a);return r;} BIND(name##W,4)
KERNEL_ONE(DeleteFile,BOOL,FALSE);
KERNEL_ONE(RemoveDirectory,BOOL,FALSE);
KERNEL_ONE(GetFileAttributes,DWORD,INVALID_FILE_ATTRIBUTES);
KERNEL_ONE(LoadLibrary,HMODULE,NULL);
KERNEL_ONE(GetModuleHandle,HMODULE,NULL);
static BOOL WINAPI setup_CreateDirectoryW(LPCWSTR path,LPSECURITY_ATTRIBUTES sa){__typeof__(&CreateDirectoryW) fn=NATIVE("kernel32.dll",CreateDirectoryW);char *a;BOOL r;if(nt())return fn(path,sa);a=ansi(path);if(!a)return FALSE;r=CreateDirectoryA(a,sa);release(a);return r;}BIND(CreateDirectoryW,8);
static BOOL WINAPI setup_SetFileAttributesW(LPCWSTR path,DWORD attrs){__typeof__(&SetFileAttributesW) fn=NATIVE("kernel32.dll",SetFileAttributesW);char *a;BOOL r;if(nt())return fn(path,attrs);a=ansi(path);if(!a)return FALSE;r=SetFileAttributesA(a,attrs);release(a);return r;}BIND(SetFileAttributesW,8);
static BOOL WINAPI setup_CopyFileW(LPCWSTR from,LPCWSTR to,BOOL exclusive){__typeof__(&CopyFileW) fn=NATIVE("kernel32.dll",CopyFileW);char *a,*b;BOOL r;if(nt())return fn(from,to,exclusive);a=ansi(from);b=ansi(to);if(!a||!b){release(a);release(b);return FALSE;}r=CopyFileA(a,b,exclusive);release(a);release(b);return r;}BIND(CopyFileW,12);
static BOOL WINAPI setup_MoveFileW(LPCWSTR from,LPCWSTR to){__typeof__(&MoveFileW) fn=NATIVE("kernel32.dll",MoveFileW);char *a,*b;BOOL r;if(nt())return fn(from,to);a=ansi(from);b=ansi(to);if(!a||!b){release(a);release(b);return FALSE;}r=MoveFileA(a,b);release(a);release(b);return r;}BIND(MoveFileW,8);
static HANDLE WINAPI setup_CreateFileW(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES sa,DWORD disposition,DWORD flags,HANDLE templateFile){__typeof__(&CreateFileW) fn=NATIVE("kernel32.dll",CreateFileW);char *a;HANDLE r;if(nt())return fn(path,access,share,sa,disposition,flags,templateFile);a=ansi(path);if(!a)return INVALID_HANDLE_VALUE;r=CreateFileA(a,access,share,sa,disposition,flags,templateFile);release(a);return r;}BIND(CreateFileW,28);
static LPWSTR WINAPI setup_GetCommandLineW(void){__typeof__(&GetCommandLineW) fn=NATIVE("kernel32.dll",GetCommandLineW);static WCHAR *text;const char *a;int n;if(nt())return fn();if(text)return text;a=GetCommandLineA();n=MultiByteToWideChar(CP_ACP,0,a,-1,NULL,0);text=HeapAlloc(GetProcessHeap(),0,n*2);if(text)MultiByteToWideChar(CP_ACP,0,a,-1,text,n);return text;}BIND(GetCommandLineW,0);
static DWORD WINAPI setup_GetModuleFileNameW(HMODULE module,LPWSTR out,DWORD cap){__typeof__(&GetModuleFileNameW) fn=NATIVE("kernel32.dll",GetModuleFileNameW);char a[MAX_PATH];DWORD n;if(nt())return fn(module,out,cap);n=GetModuleFileNameA(module,a,sizeof(a));if(!n||n>=sizeof(a)){SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}return wideOutput(a,out,cap,FALSE);}BIND(GetModuleFileNameW,12);
#define KERNEL_DIRECTORY(name) static UINT WINAPI setup_##name##W(LPWSTR out,UINT cap){__typeof__(&name##W) fn=NATIVE("kernel32.dll",name##W);char a[MAX_PATH];UINT n;if(nt())return fn(out,cap);n=name##A(a,sizeof(a));if(!n||n>=sizeof(a))return 0;return wideOutput(a,out,cap,TRUE);} BIND(name##W,8)
KERNEL_DIRECTORY(GetWindowsDirectory);
KERNEL_DIRECTORY(GetSystemDirectory);
static DWORD WINAPI setup_GetTempPathW(DWORD cap,LPWSTR out)
{
  __typeof__(&GetTempPathW) fn=NATIVE("kernel32.dll",GetTempPathW);
  char a[MAX_PATH];DWORD n;
  if(nt())return fn(cap,out);
  n=GetTempPathA(sizeof(a),a);if(!n||n>=sizeof(a))return 0;
  /* Win32s can return TEMP without the trailing separator promised by Win32.
     Callers append a filename directly, so normalize it before conversion. */
  n=lstrlenA(a);
  if(n&&a[n-1]!='\\'&&a[n-1]!='/'){
    if(n+1>=sizeof(a)){SetLastError(ERROR_FILENAME_EXCED_RANGE);return 0;}
    a[n++]='\\';a[n]=0;
  }
  return wideOutput(a,out,cap,TRUE);
}BIND(GetTempPathW,8);
static DWORD WINAPI setup_GetFullPathNameW(LPCWSTR path,DWORD cap,LPWSTR out,LPWSTR *part){__typeof__(&GetFullPathNameW) fn=NATIVE("kernel32.dll",GetFullPathNameW);char *a,temp[MAX_PATH],*filePart;DWORD n;if(nt())return fn(path,cap,out,part);if(part)*part=NULL;a=ansi(path);if(!a)return 0;n=GetFullPathNameA(a,sizeof(temp),temp,&filePart);release(a);if(!n||n>=sizeof(temp))return 0;n=wideOutput(temp,out,cap,TRUE);if(n<cap&&part&&filePart){int chars=MultiByteToWideChar(CP_ACP,0,temp,(int)(filePart-temp),NULL,0);*part=out+chars;}return n;}BIND(GetFullPathNameW,16);
static DWORD WINAPI setup_GetEnvironmentVariableW(LPCWSTR name,LPWSTR out,DWORD cap){__typeof__(&GetEnvironmentVariableW) fn=NATIVE("kernel32.dll",GetEnvironmentVariableW);char *a,*value;DWORD size,n;if(nt())return fn(name,out,cap);a=ansi(name);if(!a)return 0;size=GetEnvironmentVariableA(a,NULL,0);if(!size){release(a);return 0;}value=HeapAlloc(GetProcessHeap(),0,size);if(!value){release(a);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}n=GetEnvironmentVariableA(a,value,size);release(a);if(!n||n>=size){release(value);return 0;}n=wideOutput(value,out,cap,TRUE);release(value);return n;}BIND(GetEnvironmentVariableW,12);
static DWORD WINAPI setup_ExpandEnvironmentStringsW(LPCWSTR source,LPWSTR out,DWORD cap){__typeof__(&ExpandEnvironmentStringsW) fn=NATIVE("kernel32.dll",ExpandEnvironmentStringsW);char *a,*value;DWORD size,n;if(nt())return fn(source,out,cap);a=ansi(source);if(!a)return 0;size=ExpandEnvironmentStringsA(a,NULL,0);if(!size){release(a);return 0;}value=HeapAlloc(GetProcessHeap(),0,size);if(!value){release(a);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}n=ExpandEnvironmentStringsA(a,value,size);release(a);if(!n||n>size){release(value);return 0;}n=MultiByteToWideChar(CP_ACP,0,value,-1,NULL,0);if(out&&cap>=n)MultiByteToWideChar(CP_ACP,0,value,-1,out,cap);release(value);return n;}BIND(ExpandEnvironmentStringsW,12);
static BOOL WINAPI setup_SetWindowTextW(HWND w,LPCWSTR value){__typeof__(&SetWindowTextW) fn=NATIVE("user32.dll",SetWindowTextW);char *a;BOOL r;if(nt())return fn(w,value);a=ansi(value);if(value&&!a)return FALSE;r=SetWindowTextA(w,a);release(a);return r;}BIND(SetWindowTextW,8);
static BOOL WINAPI setup_SetDlgItemTextW(HWND w,int id,LPCWSTR value){__typeof__(&SetDlgItemTextW) fn=NATIVE("user32.dll",SetDlgItemTextW);if(nt())return fn(w,id,value);return setup_SetWindowTextW(GetDlgItem(w,id),value);}BIND(SetDlgItemTextW,12);
static UINT WINAPI setup_GetDlgItemTextW(HWND w,int id,LPWSTR out,int cap){__typeof__(&GetDlgItemTextW) fn=NATIVE("user32.dll",GetDlgItemTextW);int size;char *a;UINT r;if(nt())return fn(w,id,out,cap);if(cap<=0)return 0;size=GetWindowTextLengthA(GetDlgItem(w,id))+1;a=HeapAlloc(GetProcessHeap(),0,size);if(!a)return 0;GetDlgItemTextA(w,id,a,size);r=wideOutput(a,out,cap,FALSE);release(a);return r;}BIND(GetDlgItemTextW,16);
static DWORD WINAPI setup_FormatMessageW(DWORD flags,LPCVOID source,DWORD id,DWORD language,LPWSTR out,DWORD cap,va_list *args){__typeof__(&FormatMessageW) fn=NATIVE("kernel32.dll",FormatMessageW);char *aSource=NULL,*message=NULL;DWORD n;WCHAR *wide;if(nt())return fn(flags,source,id,language,out,cap,args);/* Installer error paths use system messages with inserts ignored. */if(!(flags&FORMAT_MESSAGE_IGNORE_INSERTS)){SetLastError(ERROR_NOT_SUPPORTED);return 0;}if(flags&FORMAT_MESSAGE_FROM_STRING){aSource=ansi(source);if(!aSource)return 0;source=aSource;}n=FormatMessageA((flags|FORMAT_MESSAGE_ALLOCATE_BUFFER),source,id,language,(LPSTR)&message,0,NULL);release(aSource);if(!n)return 0;n=MultiByteToWideChar(CP_ACP,0,message,-1,NULL,0);if(flags&FORMAT_MESSAGE_ALLOCATE_BUFFER){DWORD chars=cap>n?cap:n;wide=LocalAlloc(LMEM_FIXED,chars*2);if(!wide){LocalFree(message);return 0;}*(LPWSTR*)out=wide;cap=chars;}else wide=out;if(!wide||cap<n){LocalFree(message);SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}MultiByteToWideChar(CP_ACP,0,message,-1,wide,cap);LocalFree(message);return n-1;}BIND(FormatMessageW,28);

static LONG WINAPI setup_RegOpenKeyExW(HKEY key,LPCWSTR name,DWORD options,REGSAM access,PHKEY result)
{
  char *a;LONG r;if(nt())return NATIVE("advapi32.dll",RegOpenKeyExW)(key,name,options,access,result);
  a=ansi(name);if(name&&!a)return GetLastError();r=RegOpenKeyExA(key,a,options,access,result);release(a);return r;
}BIND(RegOpenKeyExW,20);
static LONG WINAPI setup_RegCreateKeyExW(HKEY key,LPCWSTR name,DWORD reserved,LPWSTR cls,DWORD options,REGSAM access,LPSECURITY_ATTRIBUTES sa,PHKEY result,LPDWORD disposition)
{
  char *a,*c;LONG r;if(nt())return NATIVE("advapi32.dll",RegCreateKeyExW)(key,name,reserved,cls,options,access,sa,result,disposition);
  a=ansi(name);c=ansi(cls);if((name&&!a)||(cls&&!c)){release(a);release(c);return GetLastError();}
  r=RegCreateKeyExA(key,a,reserved,c,options,access,sa,result,disposition);release(a);release(c);return r;
}BIND(RegCreateKeyExW,36);
#define REG_DELETE(name) static LONG WINAPI setup_##name##W(HKEY key,LPCWSTR value){char *a;LONG r;if(nt())return NATIVE("advapi32.dll",name##W)(key,value);a=ansi(value);if(value&&!a)return GetLastError();r=name##A(key,a);release(a);return r;}BIND(name##W,8)
REG_DELETE(RegDeleteKey);
REG_DELETE(RegDeleteValue);
static BOOL stringType(DWORD type){return type==REG_SZ||type==REG_EXPAND_SZ||type==REG_MULTI_SZ;}
static LONG WINAPI setup_RegSetValueExW(HKEY key,LPCWSTR name,DWORD reserved,DWORD type,const BYTE *data,DWORD bytes)
{
  char *a,*converted=NULL;int size;BOOL used=FALSE;LONG r;
  if(nt())return NATIVE("advapi32.dll",RegSetValueExW)(key,name,reserved,type,data,bytes);
  a=ansi(name);if(name&&!a)return GetLastError();
  if(stringType(type)&&bytes){
    if(!data||(bytes&1)||bytes/2>0x7fffffff){release(a);return ERROR_INVALID_PARAMETER;}
    size=WideCharToMultiByte(CP_ACP,0,(LPCWSTR)data,bytes/2,NULL,0,NULL,&used);
    if(!size||used){release(a);return ERROR_NO_UNICODE_TRANSLATION;}
    converted=HeapAlloc(GetProcessHeap(),0,size);if(!converted){release(a);return ERROR_NOT_ENOUGH_MEMORY;}
    if(!WideCharToMultiByte(CP_ACP,0,(LPCWSTR)data,bytes/2,converted,size,NULL,&used)||used){release(a);release(converted);return ERROR_NO_UNICODE_TRANSLATION;}
    data=(const BYTE*)converted;bytes=size;
  }
  r=RegSetValueExA(key,a,reserved,type,data,bytes);release(converted);release(a);return r;
}BIND(RegSetValueExW,24);
static LONG WINAPI setup_RegQueryValueExW(HKEY key,LPCWSTR name,LPDWORD reserved,LPDWORD type,LPBYTE data,LPDWORD bytes)
{
  char *a;BYTE *buffer=NULL;DWORD size=0,t=0,capacity=bytes?*bytes:0;LONG r;int chars;unsigned attempt;
  if(nt())return NATIVE("advapi32.dll",RegQueryValueExW)(key,name,reserved,type,data,bytes);
  if(reserved||(data&&!bytes))return ERROR_INVALID_PARAMETER;
  a=ansi(name);if(name&&!a)return GetLastError();
  /* Fetch the complete value before conversion. A byte count from the ANSI
     API cannot be used as a Unicode character count, including MULTI_SZ. */
  for(attempt=0;attempt<8;attempt++){
    r=RegQueryValueExA(key,a,NULL,&t,NULL,&size);if(r!=ERROR_SUCCESS)break;
    if(size>16*1024*1024){r=ERROR_NOT_ENOUGH_MEMORY;break;}
    release(buffer);buffer=HeapAlloc(GetProcessHeap(),0,size?size:1);if(!buffer){r=ERROR_NOT_ENOUGH_MEMORY;break;}
    r=RegQueryValueExA(key,a,NULL,&t,buffer,&size);if(r!=ERROR_MORE_DATA)break;
  }
  if(r==ERROR_SUCCESS){
    DWORD required=size;
    chars=0;
    if(stringType(t)&&size){chars=MultiByteToWideChar(CP_ACP,0,(const char*)buffer,size,NULL,0);if(!chars)r=ERROR_NO_UNICODE_TRANSLATION;required=(DWORD)chars*2;}
    if(type)*type=t;
    if(bytes)*bytes=required;
    if(r==ERROR_SUCCESS&&data){
      if(capacity<required)r=ERROR_MORE_DATA;
      else if(stringType(t)&&size)MultiByteToWideChar(CP_ACP,0,(const char*)buffer,size,(LPWSTR)data,chars);
      else if(size)CopyMemory(data,buffer,size);
    }
  }
  release(buffer);release(a);return r;
}BIND(RegQueryValueExW,24);
#ifndef Z7_SETUP_EXTERNAL_PROCESS
static BOOL WINAPI setup_CreateProcessW(LPCWSTR app,LPWSTR command,LPSECURITY_ATTRIBUTES psa,LPSECURITY_ATTRIBUTES tsa,BOOL inherit,DWORD flags,LPVOID environment,LPCWSTR directory,LPSTARTUPINFOW startup,LPPROCESS_INFORMATION process)
{
  STARTUPINFOA si;char *a,*c,*d,*desktop,*title;BOOL r;
  if(nt())return NATIVE("kernel32.dll",CreateProcessW)(app,command,psa,tsa,inherit,flags,environment,directory,startup,process);
  if(!startup||startup->cb<sizeof(*startup)||(flags&CREATE_UNICODE_ENVIRONMENT)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  a=ansi(app);c=ansi(command);d=ansi(directory);desktop=ansi(startup->lpDesktop);title=ansi(startup->lpTitle);
  if((app&&!a)||(command&&!c)||(directory&&!d)||(startup->lpDesktop&&!desktop)||(startup->lpTitle&&!title)){r=FALSE;goto done;}
  CopyMemory(&si,startup,sizeof(si));si.cb=sizeof(si);si.lpDesktop=desktop;si.lpTitle=title;
  r=CreateProcessA(a,c,psa,tsa,inherit,flags,environment,d, &si,process);
 done:release(a);release(c);release(d);release(desktop);release(title);return r;
}BIND(CreateProcessW,40);
#endif

#ifdef NT9X_SETUP_DIAGNOSTICS
static void trace(const char *message){DWORD n;HANDLE file=CreateFileA("C:\\SETTRACE.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,0,NULL);if(file!=INVALID_HANDLE_VALUE){SetFilePointer(file,0,NULL,FILE_END);WriteFile(file,message,lstrlenA(message),&n,NULL);WriteFile(file,"\r\n",2,&n,NULL);FlushFileBuffers(file);CloseHandle(file);}}
#else
#define trace(message) ((void)0)
#endif
/* Windows 95 processes WININIT.INI before loading the GUI. Preserve existing
   rename entries (including duplicate NUL deletion destinations). */
static BOOL scheduleRename(const char *from,const char *to)
{
  __typeof__(&GetShortPathNameA) shortPath=NATIVE("kernel32.dll",GetShortPathNameA);
  char ini[MAX_PATH],source[MAX_PATH],target[MAX_PATH],parent[MAX_PATH];
  char *entries=NULL,*p;DWORD n,used,error=0;HANDLE mutex;BOOL result=FALSE;
  trace("schedule start");
  if(!shortPath){SetLastError(ERROR_PROC_NOT_FOUND);return FALSE;}
  if(!shortPath(from,source,MAX_PATH))return FALSE;
  if(!to&&(GetFileAttributesA(from)&FILE_ATTRIBUTE_DIRECTORY)){
    /* WININIT removes files, but not directories. RunOnce runs after its file
       cleanup. RD only removes an empty directory and never recurses. */
    char command[2*MAX_PATH+32],name[48];DWORD length=lstrlenA(source);HKEY key;LONG status;
    while(length&&source[length-1]=='\\')source[--length]=0;
    if(length<=3){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    char processor[MAX_PATH];DWORD chars=GetEnvironmentVariableA("COMSPEC",processor,MAX_PATH);
    if(!chars||chars>=MAX_PATH||strchr(processor,'"')||GetFileAttributesA(processor)==INVALID_FILE_ATTRIBUTES){SetLastError(ERROR_FILE_NOT_FOUND);return FALSE;}
    wsprintfA(command,"\"%s\" /c rd \"%s\"",processor,source);
    wsprintfA(name,"7ZipTempCleanup-%08lX",GetTickCount()^GetCurrentProcessId());
    status=RegCreateKeyExA(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL);
    if(!status){status=RegSetValueExA(key,name,0,REG_SZ,(const BYTE*)command,lstrlenA(command)+1);RegCloseKey(key);}
    if(status)SetLastError(status);return status==ERROR_SUCCESS;
  }
  if(!to)lstrcpyA(target,"NUL");
  else if(!shortPath(to,target,MAX_PATH)){
    const char *base;int len;lstrcpynA(parent,to,MAX_PATH);p=strrchr(parent,'\\');
    if(!p){SetLastError(ERROR_BAD_PATHNAME);return FALSE;}base=to+(p-parent)+1;*p=0;
    n=shortPath(parent,target,MAX_PATH);len=lstrlenA(base);
    if(!n||n+len+2>=MAX_PATH)return FALSE;
    /* The pending replacement's leaf must itself be an 8.3 name. */
    if(strchr(base,' ')||len>12){SetLastError(ERROR_INVALID_NAME);return FALSE;}
    lstrcatA(target,"\\");lstrcatA(target,base);
  }
  n=GetWindowsDirectoryA(ini,MAX_PATH);if(!n||n+13>=MAX_PATH)return FALSE;lstrcatA(ini,"\\WININIT.INI");
  trace("short paths ready");
  mutex=CreateMutexA(NULL,FALSE,"7ZipLegacyWininit");if(!mutex)return FALSE;
  n=WaitForSingleObject(mutex,10000);if(n!=WAIT_OBJECT_0&&n!=WAIT_ABANDONED){CloseHandle(mutex);SetLastError(ERROR_BUSY);return FALSE;}
  trace("mutex held");
  entries=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,32767);if(!entries){error=ERROR_NOT_ENOUGH_MEMORY;goto done;}
  /* The Win95 profile thunk uses a 16-bit signed buffer count. */
  trace("read rename section");
  n=GetPrivateProfileSectionA("Rename",entries,32767,ini);if(n>=32765){error=ERROR_INSUFFICIENT_BUFFER;goto done;}
  p=entries;while(*p)p+=lstrlenA(p)+1;used=(DWORD)(p-entries);
  if(used+lstrlenA(target)+lstrlenA(source)+3>=32767){error=ERROR_INSUFFICIENT_BUFFER;goto done;}
  lstrcpyA(p,target);lstrcatA(p,"=");lstrcatA(p,source);p[lstrlenA(p)+1]=0;
  trace("write rename section");
  result=WritePrivateProfileSectionA("Rename",entries,ini);if(!result)error=GetLastError();
  trace("flush rename section");
  if(result)WritePrivateProfileStringA(NULL,NULL,NULL,ini);
 done:trace("schedule finished");release(entries);ReleaseMutex(mutex);CloseHandle(mutex);if(!result)SetLastError(error);return result;
}
static BOOL WINAPI setup_MoveFileExW(LPCWSTR from,LPCWSTR to,DWORD flags)
{
  char *a,*b;BOOL r=FALSE;DWORD error;
  if(nt())return NATIVE("kernel32.dll",MoveFileExW)(from,to,flags);
  if(flags&~(MOVEFILE_REPLACE_EXISTING|MOVEFILE_COPY_ALLOWED|MOVEFILE_DELAY_UNTIL_REBOOT|MOVEFILE_WRITE_THROUGH)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  a=ansi(from);b=ansi(to);if(!a||(to&&!b)){release(a);release(b);return FALSE;}
  if(flags&MOVEFILE_DELAY_UNTIL_REBOOT){
    if(flags&MOVEFILE_COPY_ALLOWED)SetLastError(ERROR_INVALID_PARAMETER);
    else r=scheduleRename(a,b);
  }else if(!b)SetLastError(ERROR_INVALID_PARAMETER);
  else{
    char fullSource[MAX_PATH],fullTarget[MAX_PATH];DWORD sourceLength,targetLength,sourceAttrs=GetFileAttributesA(a);
    if(sourceAttrs==INVALID_FILE_ATTRIBUTES){error=GetLastError();release(a);release(b);SetLastError(error);return FALSE;}
    sourceLength=GetFullPathNameA(a,MAX_PATH,fullSource,NULL);
    targetLength=GetFullPathNameA(b,MAX_PATH,fullTarget,NULL);
    if(sourceLength&&sourceLength<MAX_PATH&&targetLength&&targetLength<MAX_PATH&&!lstrcmpiA(fullSource,fullTarget))r=TRUE;
    else r=MoveFileA(a,b);
    error=GetLastError();
    if(!r&&(flags&MOVEFILE_REPLACE_EXISTING)&&!(sourceAttrs&FILE_ATTRIBUTE_DIRECTORY)&&
        GetFileAttributesA(b)!=INVALID_FILE_ATTRIBUTES&&!(GetFileAttributesA(b)&FILE_ATTRIBUTE_DIRECTORY)){
      char parent[MAX_PATH],backup[MAX_PATH],*leaf=NULL;
      DWORD length=GetFullPathNameA(b,MAX_PATH,parent,&leaf);
      if(length&&length<MAX_PATH&&leaf){
        *leaf=0;
        if(GetTempFileNameA(parent,"7ZR",0,backup)){
          DeleteFileA(backup);
          if(MoveFileA(b,backup)){
            r=MoveFileA(a,b);error=GetLastError();
            if(r)DeleteFileA(backup);
            else if(!MoveFileA(backup,b))error=ERROR_WRITE_FAULT;
          }else error=GetLastError();
        }else error=GetLastError();
      }
    }
    if(!r&&error==ERROR_NOT_SAME_DEVICE&&(flags&MOVEFILE_COPY_ALLOWED)){r=CopyFileA(a,b,!(flags&MOVEFILE_REPLACE_EXISTING));if(r)r=DeleteFileA(a);error=GetLastError();}
    SetLastError(error);
  }
  error=GetLastError();release(a);release(b);SetLastError(error);return r;
}BIND(MoveFileExW,12);
/* MinGW's WinMain adapter uses this CRT helper, absent from NT 3.1 CRTDLL. */
#ifndef Z7_SETUP_EXTERNAL_PROCESS
int __cdecl _ismbblead(unsigned int value){return value<=255&&IsDBCSLeadByte((BYTE)value);}
/* Also satisfy CRT adapters compiled with the dllimport declaration. */
int (__cdecl *setup_ismbblead_import)(unsigned int) __asm__("__imp___ismbblead") = _ismbblead;
#endif
/* Windows 95 exports several W string routines as nonfunctional stubs too.
   These operate on UTF-16 code units and need no code-page conversion. */
static int WINAPI setup_lstrlenW(LPCWSTR s){int n=0;if(s)while(s[n])n++;return n;}BIND(lstrlenW,4);
static LPWSTR WINAPI setup_lstrcpyW(LPWSTR target,LPCWSTR source){WCHAR *p=target;if(!target||!source){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}while((*p++=*source++)!=0){}return target;}BIND(lstrcpyW,8);
static LPWSTR WINAPI setup_lstrcatW(LPWSTR target,LPCWSTR source){WCHAR *p=target;if(!target||!source){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}while(*p)p++;while((*p++=*source++)!=0){}return target;}BIND(lstrcatW,8);
static int compareText(LPCWSTR left,LPCWSTR right,BOOL ignoreCase){char *a,*b;int result;if(!left||!right)return left?1:right?-1:0;a=ansi(left);b=ansi(right);if(a&&b)result=ignoreCase?lstrcmpiA(a,b):lstrcmpA(a,b);else{while(*left&&*left==*right){left++;right++;}result=*left>*right?1:*left<*right?-1:0;}release(a);release(b);return result;}
static int WINAPI setup_lstrcmpW(LPCWSTR left,LPCWSTR right){if(nt())return NATIVE("kernel32.dll",lstrcmpW)(left,right);return compareText(left,right,FALSE);}BIND(lstrcmpW,8);
static int WINAPI setup_lstrcmpiW(LPCWSTR left,LPCWSTR right){if(nt())return NATIVE("kernel32.dll",lstrcmpiW)(left,right);return compareText(left,right,TRUE);}BIND(lstrcmpiW,8);
