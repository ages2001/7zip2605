/* Behavioral tests for the i386 runtime and NT/9x synchronization layer.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <oleauto.h>
#include <stdint.h>
static CRITICAL_SECTION guard;
static volatile unsigned atomic_count;
static unsigned protected_count;
static HANDLE report;
static void line(const char *text) {
  DWORD written;
  WriteFile(report,text,lstrlenA(text),&written,NULL);
}
static void require(bool condition,const char *message) {
  if(!condition){line("FAIL: ");line(message);line("\r\n");ExitProcess(1);}
}
static DWORD WINAPI worker(void *mode) {
  for(unsigned i=0;i<1000;++i) {
    __sync_add_and_fetch(&atomic_count,1);
    EnterCriticalSection(&guard);
    EnterCriticalSection(&guard);
    ++protected_count;
    LeaveCriticalSection(&guard);
    LeaveCriticalSection(&guard);
    // Exercise per-thread C++ exception state and frame unwinding.
    if (mode) {
      try { throw (int)i; }
      catch(int value) { if(value!=(int)i)return 1; }
    }
  }
  return 0;
}
struct Cleanup { int &value; Cleanup(int &v):value(v){} ~Cleanup(){++value;} };
static void unwind(int &value) { Cleanup cleanup(value); throw 42; }
int main() {
  HANDLE threads[4];
  report=CreateFileA("C:\\RUNTIME.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if(report==INVALID_HANDLE_VALUE)return 2;
  line("i386 runtime tests\r\n");
  int cleaned=0;
  try { unwind(cleaned); require(false,"throw returned"); }
  catch(int value) { require(value==42&&cleaned==1,"exception and destructor unwinding"); }
  line("PASS exception and destructor unwinding\r\n");
  unsigned value=7;
  require(__sync_fetch_and_add(&value,5)==7&&value==12,"fetch-and-add");
  require(__sync_val_compare_and_swap(&value,7,9)==12&&value==12,"failed compare-and-swap");
  require(__sync_bool_compare_and_swap(&value,12,9)&&value==9,"successful compare-and-swap");
  require(__sync_sub_and_fetch(&value,3)==6,"subtract-and-fetch");
  InitializeCriticalSection(&guard);
  for(unsigned i=0;i<4;++i) {
    threads[i]=CreateThread(NULL,65536,worker,(void *)1,0,NULL);
    require(threads[i]!=NULL,"CreateThread");
  }
  DWORD waited=WaitForMultipleObjects(4,threads,TRUE,180000);
  char number[11]="00000000\r\n";
  DWORD diagnostic=waited==WAIT_FAILED?GetLastError():waited;
  for(unsigned j=0;j<8;++j)number[j]="0123456789ABCDEF"[(diagnostic>>(28-4*j))&15];
  line("Wait result/error: ");line(number);
  // NT 3.1 can return the index of the last satisfied handle for wait-all.
  require(waited < WAIT_OBJECT_0 + 4,"thread completion");
  for(unsigned i=0;i<4;++i) {
    require(WaitForSingleObject(threads[i],0)==WAIT_OBJECT_0,"thread still running after wait-all");
    DWORD code=99;
    require(GetExitCodeThread(threads[i],&code)&&code==0,"thread exception state");
    CloseHandle(threads[i]);
  }
  require(atomic_count==4000&&protected_count==4000,"concurrent counters");
  DeleteCriticalSection(&guard);
  line("PASS software atomics, recursive critical sections and concurrent exceptions\r\n");
  VARIANT a,b;ZeroMemory(&a,sizeof(a));ZeroMemory(&b,sizeof(b));
  const OLECHAR embedded[]={L'A',0,L'B'};
  a.vt=VT_BSTR;a.bstrVal=SysAllocStringLen(embedded,3);
  require(a.bstrVal!=NULL&&SysStringLen(a.bstrVal)==3,"BSTR allocation");
  require(VariantCopy(&b,&a)==S_OK&&b.bstrVal!=a.bstrVal,"BSTR copy ownership");
  require(VariantClear(&a)==S_OK&&b.bstrVal[2]==L'B'&&SysStringLen(b.bstrVal)==3,"embedded NUL preservation");
  require(VariantClear(&b)==S_OK,"BSTR cleanup");
  a.vt=VT_BSTR;a.bstrVal=SysAllocStringByteLen("A\0B",3);
  require(a.bstrVal&&SysStringByteLen(a.bstrVal)==3&&SysStringLen(a.bstrVal)==1,"odd-length binary BSTR allocation");
  require(VariantCopy(&b,&a)==S_OK&&SysStringByteLen(b.bstrVal)==3&&!memcmp(b.bstrVal,"A\0B",3),"binary BSTR deep copy");
  require(VariantClear(&a)==S_OK&&VariantClear(&b)==S_OK,"binary BSTR cleanup");
  {HMODULE native=GetModuleHandleA("oleaut32.dll");if(native){
    typedef BSTR (WINAPI *Allocate)(LPCSTR,UINT);Allocate allocate=(Allocate)GetProcAddress(native,"SysAllocStringByteLen");
    BSTR value=allocate?allocate("N\0T",3):NULL;
    require(value&&SysStringByteLen(value)==3,"native automation allocation interoperates");SysFreeString(value);
  }}
  line("PASS text and binary BSTR ownership, including native automation interoperability\r\nPASS ALL\r\n");
  CloseHandle(report);return 0;
}
