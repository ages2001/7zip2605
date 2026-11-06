/* NT 3.1 process snapshots backed by real kernel section handles.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#define BIND(name, bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes) = legacy_##name
#define NATIVE(name) ((__typeof__(&name))GetProcAddress(GetModuleHandleA("kernel32.dll"),#name))
/* NT 3.1 omits inherited PID from its public process information. Remember
   children actually started by this process, retaining a handle so PID reuse
   cannot turn an unrelated process into a child. Unobserved parents remain 0. */
typedef struct Child {struct Child *next;DWORD id,parent;HANDLE handle;} Child;
static Child *children;
static LONG children_lock;
static void lock_children(void){while(InterlockedExchange(&children_lock,1))Sleep(0);}
static void unlock_children(void){InterlockedExchange(&children_lock,0);}
static void remember_child(const PROCESS_INFORMATION *info){
 Child *child;
 if(NATIVE(CreateToolhelp32Snapshot))return;
 child=HeapAlloc(GetProcessHeap(),0,sizeof(*child));if(!child)return;
 if(!DuplicateHandle(GetCurrentProcess(),info->hProcess,GetCurrentProcess(),&child->handle,0,FALSE,DUPLICATE_SAME_ACCESS)){HeapFree(GetProcessHeap(),0,child);return;}
 child->id=info->dwProcessId;child->parent=GetCurrentProcessId();
 lock_children();child->next=children;children=child;unlock_children();
}
static DWORD remembered_parent(DWORD pid){
 Child **link,*child;DWORD result=0,exit_code;
 lock_children();link=&children;
 while((child=*link)!=NULL){
  if(GetExitCodeProcess(child->handle,&exit_code)&&exit_code!=STILL_ACTIVE){*link=child->next;CloseHandle(child->handle);HeapFree(GetProcessHeap(),0,child);continue;}
  if(child->id==pid)result=child->parent;link=&child->next;
 }
 unlock_children();return result;
}
static BOOL WINAPI legacy_CreateProcessA(LPCSTR app,LPSTR command,LPSECURITY_ATTRIBUTES process_security,LPSECURITY_ATTRIBUTES thread_security,BOOL inherit,DWORD flags,LPVOID environment,LPCSTR directory,LPSTARTUPINFOA startup,LPPROCESS_INFORMATION info){
 BOOL result=NATIVE(CreateProcessA)(app,command,process_security,thread_security,inherit,flags,environment,directory,startup,info);DWORD error=GetLastError();if(result)remember_child(info);SetLastError(error);return result;
}
BIND(CreateProcessA,40);
static BOOL WINAPI legacy_CreateProcessW(LPCWSTR app,LPWSTR command,LPSECURITY_ATTRIBUTES process_security,LPSECURITY_ATTRIBUTES thread_security,BOOL inherit,DWORD flags,LPVOID environment,LPCWSTR directory,LPSTARTUPINFOW startup,LPPROCESS_INFORMATION info){
 BOOL result=NATIVE(CreateProcessW)(app,command,process_security,thread_security,inherit,flags,environment,directory,startup,info);DWORD error=GetLastError();if(result)remember_child(info);SetLastError(error);return result;
}
BIND(CreateProcessW,40);
typedef struct NativeProcess {
 ULONG next,threads;BYTE reserved[24];LARGE_INTEGER create,user,kernel;
 struct {USHORT length,maximum;WCHAR *buffer;} name;
 LONG priority;HANDLE id,parent;
} NativeProcess;
typedef struct Snapshot {DWORD magic,count,cursor;PROCESSENTRY32 entries[1];} Snapshot;
#define SNAP_MAGIC 0x3153375aUL
HANDLE WINAPI CreateToolhelp32Snapshot(DWORD flags,DWORD pid){
 __typeof__(&CreateToolhelp32Snapshot) fn=NATIVE(CreateToolhelp32Snapshot);
 typedef LONG (WINAPI *QueryFn)(ULONG,PVOID,ULONG,PULONG);
 QueryFn query;BYTE *buffer;DWORD size=65536,needed,count,offset,total;LONG status;HANDLE handle;Snapshot *snapshot;SECURITY_ATTRIBUTES sa;
 if(fn)return fn(flags,pid);
 if((flags&~TH32CS_INHERIT)!=TH32CS_SNAPPROCESS){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return INVALID_HANDLE_VALUE;}
 query=(QueryFn)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtQuerySystemInformation");
 if(!query){SetLastError(ERROR_PROC_NOT_FOUND);return INVALID_HANDLE_VALUE;}
 for(;;){
  buffer=HeapAlloc(GetProcessHeap(),0,size);if(!buffer){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return INVALID_HANDLE_VALUE;}
  needed=0;status=query(5,buffer,size,&needed);
  if(status>=0)break;HeapFree(GetProcessHeap(),0,buffer);
  if((DWORD)status!=0xc0000004UL&&(DWORD)status!=0xc0000023UL){SetLastError(ERROR_GEN_FAILURE);return INVALID_HANDLE_VALUE;}
  if(size>=64*1024*1024){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return INVALID_HANDLE_VALUE;}size*=2;
 }
 count=0;offset=0;
 for(;;){NativeProcess *p;
  if(offset>size-sizeof(NativeProcess))goto invalid;
  p=(NativeProcess*)(buffer+offset);count++;
  if(!p->next)break;if(p->next<sizeof(NativeProcess)||p->next>size-offset)goto invalid;offset+=p->next;
 }
 if(count>(0x7fffffff-sizeof(Snapshot))/sizeof(PROCESSENTRY32))goto invalid;
 total=sizeof(Snapshot)+(count-1)*sizeof(PROCESSENTRY32);
 sa.nLength=sizeof(sa);sa.lpSecurityDescriptor=NULL;sa.bInheritHandle=(flags&TH32CS_INHERIT)!=0;
 handle=CreateFileMappingA(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,total,NULL);
 if(!handle){HeapFree(GetProcessHeap(),0,buffer);return INVALID_HANDLE_VALUE;}
 snapshot=MapViewOfFile(handle,FILE_MAP_WRITE,0,0,total);
 if(!snapshot){DWORD error=GetLastError();CloseHandle(handle);HeapFree(GetProcessHeap(),0,buffer);SetLastError(error);return INVALID_HANDLE_VALUE;}
 snapshot->magic=SNAP_MAGIC;snapshot->count=count;snapshot->cursor=0;offset=0;
 for(count=0;count<snapshot->count;count++){
  NativeProcess *p=(NativeProcess*)(buffer+offset);PROCESSENTRY32 *entry=snapshot->entries+count;int length;
  ZeroMemory(entry,sizeof(*entry));entry->dwSize=sizeof(*entry);entry->th32ProcessID=(DWORD)p->id;entry->th32ParentProcessID=(DWORD)p->parent;entry->cntThreads=p->threads;entry->pcPriClassBase=p->priority;
  if(entry->th32ProcessID && !entry->th32ParentProcessID && (GetVersion()&0xffff)!=0x0a03){
   typedef LONG (WINAPI *BasicFn)(HANDLE,ULONG,PVOID,ULONG,PULONG);
   struct {LONG status;PVOID peb;ULONG affinity;LONG priority;ULONG id,parent;} basic;
   ULONG returned=0;
   BasicFn basic_query=(BasicFn)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtQueryInformationProcess");
   HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,entry->th32ProcessID);
   ZeroMemory(&basic,sizeof(basic));
   if(process){
    if(basic_query&&basic_query(process,0,&basic,sizeof(basic),&returned)>=0&&returned>=sizeof(basic)&&basic.id==entry->th32ProcessID)entry->th32ParentProcessID=basic.parent;
#ifdef NT9X_PROCESS_DIAGNOSTICS
    if(entry->th32ProcessID==GetCurrentProcessId()){
     HANDLE out=CreateFileA("C:\\PBASIC.BIN",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);DWORD n;
     if(out!=INVALID_HANDLE_VALUE){WriteFile(out,&returned,sizeof(returned),&n,NULL);WriteFile(out,&basic,sizeof(basic),&n,NULL);FlushFileBuffers(out);CloseHandle(out);}
    }
#endif
    CloseHandle(process);
   }
  }
  if(!entry->th32ParentProcessID)entry->th32ParentProcessID=remembered_parent(entry->th32ProcessID);
  if(p->name.length){
   if((p->name.length&1)||(BYTE*)p->name.buffer<buffer||(BYTE*)p->name.buffer>buffer+size-p->name.length){UnmapViewOfFile(snapshot);CloseHandle(handle);goto invalid;}
   /* A process name cannot exceed the Toolhelp buffer. Reject instead of
      silently returning an incomplete snapshot with an empty process name. */
   length=WideCharToMultiByte(CP_ACP,0,p->name.buffer,p->name.length/2,entry->szExeFile,MAX_PATH-1,NULL,NULL);
   if(!length){DWORD error=GetLastError();UnmapViewOfFile(snapshot);CloseHandle(handle);HeapFree(GetProcessHeap(),0,buffer);SetLastError(error);return INVALID_HANDLE_VALUE;}
   entry->szExeFile[length]=0;
  }else lstrcpyA(entry->szExeFile,entry->th32ProcessID?"System":"[System Process]");
  offset+=p->next;
 }
 UnmapViewOfFile(snapshot);HeapFree(GetProcessHeap(),0,buffer);return handle;
invalid:
 HeapFree(GetProcessHeap(),0,buffer);SetLastError(ERROR_INVALID_DATA);return INVALID_HANDLE_VALUE;
}
__typeof__(&CreateToolhelp32Snapshot) legacy_import_CreateToolhelp32Snapshot __asm__("__imp__CreateToolhelp32Snapshot@8") = CreateToolhelp32Snapshot;
static BOOL next_process(HANDLE handle,PROCESSENTRY32 *entry,BOOL first){Snapshot *snapshot;MEMORY_BASIC_INFORMATION region;DWORD cursor;
 if(!entry||entry->dwSize<sizeof(*entry)){SetLastError(ERROR_BAD_LENGTH);return FALSE;}
 snapshot=MapViewOfFile(handle,FILE_MAP_WRITE,0,0,0);if(!snapshot)return FALSE;
 if(!VirtualQuery(snapshot,&region,sizeof(region))||region.RegionSize<sizeof(Snapshot)||snapshot->magic!=SNAP_MAGIC||snapshot->count>(region.RegionSize-sizeof(Snapshot))/sizeof(PROCESSENTRY32)+1){UnmapViewOfFile(snapshot);SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
 cursor=first?0:snapshot->cursor;
 if(cursor>=snapshot->count){UnmapViewOfFile(snapshot);SetLastError(ERROR_NO_MORE_FILES);return FALSE;}
 *entry=snapshot->entries[cursor];snapshot->cursor=cursor+1;UnmapViewOfFile(snapshot);return TRUE;
}
BOOL WINAPI Process32First(HANDLE handle,PROCESSENTRY32 *entry){__typeof__(&Process32First) fn=NATIVE(Process32First);return fn?fn(handle,entry):next_process(handle,entry,TRUE);}
__typeof__(&Process32First) legacy_import_Process32First __asm__("__imp__Process32First@8") = Process32First;
BOOL WINAPI Process32Next(HANDLE handle,PROCESSENTRY32 *entry){__typeof__(&Process32Next) fn=NATIVE(Process32Next);return fn?fn(handle,entry):next_process(handle,entry,FALSE);}
__typeof__(&Process32Next) legacy_import_Process32Next __asm__("__imp__Process32Next@8") = Process32Next;
static DWORD WINAPI legacy_GetCompressedFileSizeW(LPCWSTR path,LPDWORD high){
 __typeof__(&GetCompressedFileSizeW) fn=NATIVE(GetCompressedFileSizeW);DWORD attributes,low,error;HANDLE file;
 if(fn)return fn(path,high);
 attributes=GetFileAttributesW(path);if(attributes==INVALID_FILE_ATTRIBUTES)return INVALID_FILE_SIZE;
 /* NT 3.1 has no NTFS compression. Do not report a fabricated size for a
    filesystem driver that advertises compression or sparse allocation. */
 if(attributes&(FILE_ATTRIBUTE_COMPRESSED|FILE_ATTRIBUTE_SPARSE_FILE)){SetLastError(ERROR_NOT_SUPPORTED);return INVALID_FILE_SIZE;}
 file=CreateFileW(path,0,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
 if(file==INVALID_HANDLE_VALUE)return INVALID_FILE_SIZE;
 SetLastError(NO_ERROR);low=GetFileSize(file,high);error=GetLastError();CloseHandle(file);SetLastError(error);return low;
}
BIND(GetCompressedFileSizeW,8);
/* CRT startup uses the active Windows ANSI code page while scanning argv.
   Restrict to one byte: IsDBCSLeadByte takes a BYTE and would otherwise wrap. */
int __cdecl _ismbblead(unsigned int value){return value<=255&&IsDBCSLeadByte((BYTE)value)?-1:0;}
int (__cdecl *legacy_import_ismbblead)(unsigned int) __asm__("__imp___ismbblead") = _ismbblead;
