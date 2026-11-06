/* NT 3.1 / Windows 95 compatibility imports.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later
   Keep unsupported operations observable; never report a fabricated success. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define _WIN32_WINNT 0x0400
#define WINVER 0x0400
#include <windows.h>
#include <oleauto.h>
#include <stdint.h>
#include <string.h>

#define BIND(name, bytes) \
  __typeof__(&name) legacy_import_##name \
  __asm__("__imp__" #name "@" #bytes) = legacy_##name

static FARPROC kernel_proc(const char *name) {
  return GetProcAddress(GetModuleHandleA("kernel32.dll"), name);
}

static BOOL WINAPI legacy_AreFileApisANSI(void) {
  typedef BOOL (WINAPI *Fn)(void);
  Fn fn = (Fn)kernel_proc("AreFileApisANSI");
  return fn ? fn() : TRUE; /* NT 3.1 has no SetFileApisToOEM. */
}
BIND(AreFileApisANSI, 0);

static BOOL WINAPI legacy_GetVersionExA(LPOSVERSIONINFOA info) {
  typedef BOOL (WINAPI *Fn)(LPOSVERSIONINFOA);
  Fn fn = (Fn)kernel_proc("GetVersionExA");
  DWORD version;
  if (fn) return fn(info);
  if (!info || info->dwOSVersionInfoSize != sizeof(OSVERSIONINFOA)) {
    SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
  }
  version = GetVersion();
  info->dwMajorVersion = LOBYTE(LOWORD(version));
  info->dwMinorVersion = HIBYTE(LOWORD(version));
  info->dwBuildNumber = (version & 0x80000000UL) ? 0 : HIWORD(version);
  info->dwPlatformId = (version & 0x80000000UL) ? VER_PLATFORM_WIN32_WINDOWS : VER_PLATFORM_WIN32_NT;
  info->szCSDVersion[0] = 0;
  return TRUE;
}
BIND(GetVersionExA, 4);

static VOID WINAPI legacy_GetSystemTimeAsFileTime(LPFILETIME time) {
  SYSTEMTIME system_time;
  GetSystemTime(&system_time);
  SystemTimeToFileTime(&system_time, time);
}
BIND(GetSystemTimeAsFileTime, 4);

static BOOL WINAPI legacy_IsDBCSLeadByteEx(UINT code_page, BYTE value) {
  CPINFO info;
  unsigned i;
  if (!GetCPInfo(code_page, &info)) return FALSE;
  for (i = 0; i + 1 < MAX_LEADBYTES && info.LeadByte[i]; i += 2)
    if (value >= info.LeadByte[i] && value <= info.LeadByte[i + 1]) return TRUE;
  return FALSE;
}
BIND(IsDBCSLeadByteEx, 8);

static LPSTR WINAPI legacy_CharPrevExA(WORD code_page, LPCSTR start, LPCSTR current, DWORD flags) {
  LPCSTR p = start, previous = start;
  (void)flags;
  if (current <= start) return (LPSTR)start;
  while (p < current && *p) {
    previous = p;
    p += legacy_IsDBCSLeadByteEx(code_page, (BYTE)*p) && p[1] ? 2 : 1;
  }
  return (LPSTR)previous;
}
BIND(CharPrevExA, 16);

static BOOL WINAPI legacy_IsProcessorFeaturePresent(DWORD feature) {
  typedef BOOL (WINAPI *Fn)(DWORD);
  Fn fn = (Fn)kernel_proc("IsProcessorFeaturePresent");
  /* Conservative software fallback when the OS cannot report SIMD support. */
  return fn ? fn(feature) : FALSE;
}
BIND(IsProcessorFeaturePresent, 4);

static BOOL WINAPI legacy_GetProcessAffinityMask(HANDLE process, PDWORD_PTR process_mask, PDWORD_PTR system_mask) {
  typedef BOOL (WINAPI *Fn)(HANDLE, PDWORD_PTR, PDWORD_PTR);
  Fn fn = (Fn)kernel_proc("GetProcessAffinityMask");
  if (fn) return fn(process, process_mask, system_mask);
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return FALSE;
}
BIND(GetProcessAffinityMask, 12);

static BOOL WINAPI legacy_SetProcessAffinityMask(HANDLE process, DWORD_PTR mask) {
  typedef BOOL (WINAPI *Fn)(HANDLE, DWORD_PTR);
  Fn fn = (Fn)kernel_proc("SetProcessAffinityMask");
  if (fn) return fn(process, mask);
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return FALSE;
}
BIND(SetProcessAffinityMask, 8);

/* Private critical sections use recursive kernel mutexes. All five imports
   are replaced together: never mix native and private critical-section layouts.
   This favors correctness on NT 3.1/9x over a modern user-mode fast path. */
static VOID WINAPI legacy_InitializeCriticalSection(LPCRITICAL_SECTION section) {
  HANDLE mutex = CreateMutexA(NULL, FALSE, NULL);
  if (!mutex) RaiseException(STATUS_NO_MEMORY, 0, 0, NULL);
  memset(section, 0, sizeof(*section));
  section->LockSemaphore = mutex;
}
BIND(InitializeCriticalSection, 4);
static VOID WINAPI legacy_EnterCriticalSection(LPCRITICAL_SECTION section) {
  DWORD result = WaitForSingleObject(section->LockSemaphore, INFINITE);
  if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED)
    RaiseException(EXCEPTION_INVALID_HANDLE, 0, 0, NULL);
}
BIND(EnterCriticalSection, 4);
static BOOL WINAPI legacy_TryEnterCriticalSection(LPCRITICAL_SECTION section) {
  DWORD result = WaitForSingleObject(section->LockSemaphore, 0);
  return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
}
BIND(TryEnterCriticalSection, 4);
static VOID WINAPI legacy_LeaveCriticalSection(LPCRITICAL_SECTION section) {
  if (!ReleaseMutex(section->LockSemaphore))
    RaiseException(EXCEPTION_INVALID_HANDLE, 0, 0, NULL);
}
BIND(LeaveCriticalSection, 4);
static VOID WINAPI legacy_DeleteCriticalSection(LPCRITICAL_SECTION section) {
  CloseHandle(section->LockSemaphore);
  section->LockSemaphore = NULL;
}
BIND(DeleteCriticalSection, 4);

static DWORD WINAPI legacy_GetThreadId(HANDLE thread) {
  typedef DWORD (WINAPI *Fn)(HANDLE);
  typedef LONG (WINAPI *QueryFn)(HANDLE, ULONG, void *, ULONG, ULONG *);
  struct ThreadBasicInformation {
    LONG status; void *teb; HANDLE process_id; HANDLE thread_id;
    ULONG affinity; LONG priority; LONG base_priority;
  } basic;
  Fn fn = (Fn)kernel_proc("GetThreadId");
  HMODULE ntdll;
  QueryFn query;
  if (fn) return fn(thread);
  ntdll = GetModuleHandleA("ntdll.dll");
  query = ntdll ? (QueryFn)GetProcAddress(ntdll, "NtQueryInformationThread") : NULL;
  if (query && query(thread, 0, &basic, sizeof(basic), NULL) >= 0)
    return (DWORD)(ULONG_PTR)basic.thread_id;
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return 0;
}
/* GetThreadId is not declared at a Windows 4.0 SDK target. */
DWORD (WINAPI *legacy_import_GetThreadId)(HANDLE)
  __asm__("__imp__GetThreadId@4") = legacy_GetThreadId;

static HANDLE WINAPI legacy_CreateThread(LPSECURITY_ATTRIBUTES attributes, SIZE_T stack_size,
    LPTHREAD_START_ROUTINE start, LPVOID argument, DWORD flags, LPDWORD thread_id) {
  typedef HANDLE (WINAPI *Fn)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
  Fn fn = (Fn)kernel_proc("CreateThread");
  DWORD version = GetVersion();
  DWORD local_id;
  /* The reservation flag was introduced after both supported targets. */
  if (LOBYTE(LOWORD(version)) < 5)
    flags &= ~STACK_SIZE_PARAM_IS_A_RESERVATION;
  return fn(attributes, stack_size, start, argument, flags, thread_id ? thread_id : &local_id);
}
BIND(CreateThread, 24);

/* Prefer the native automation allocator whenever available, so external
   codec DLLs and the front ends share BSTR ownership on Windows 95 / NT4.
   NT 3.1 modules built with this port share the process-heap fallback. */
static FARPROC automation(const char *name) {
  static HMODULE module;static LONG lock;FARPROC result;
  DWORD version=GetVersion();
  if(!(version&0x80000000UL)&&(version&255)==3&&((version>>8)&255)<50)return NULL;
  while(InterlockedExchange(&lock,1))Sleep(0);
  if(!module){module=LoadLibraryA("oleaut32.dll");if(!module)module=(HMODULE)1;}
  result=module==(HMODULE)1?NULL:GetProcAddress(module,name);
  InterlockedExchange(&lock,0);return result;
}
static BSTR WINAPI legacy_SysAllocStringLen(const OLECHAR *source, UINT length) {
  __typeof__(&SysAllocStringLen) native=(__typeof__(&SysAllocStringLen))automation("SysAllocStringLen");
  if(native)return native(source, length);
  DWORD *block;
  SIZE_T bytes;
  BSTR value;
  if (length > (UINT_MAX - sizeof(DWORD) - sizeof(OLECHAR)) / sizeof(OLECHAR)) return NULL;
  bytes = (SIZE_T)length * sizeof(OLECHAR);
  block = HeapAlloc(GetProcessHeap(), 0, sizeof(DWORD) + bytes + sizeof(OLECHAR));
  if (!block) return NULL;
  *block = (DWORD)bytes;
  value = (BSTR)(block + 1);
  if (source && bytes) memcpy(value, source, bytes);
  value[length] = 0;
  return value;
}
BIND(SysAllocStringLen, 8);
static BSTR WINAPI legacy_SysAllocString(const OLECHAR *source) {
  __typeof__(&SysAllocString) native=(__typeof__(&SysAllocString))automation("SysAllocString");
  if(native)return native(source);
  UINT length = 0;
  if (!source) return NULL;
  while (source[length]) ++length;
  return legacy_SysAllocStringLen(source, length);
}
BIND(SysAllocString, 4);
static VOID WINAPI legacy_SysFreeString(BSTR value) {
  __typeof__(&SysFreeString) native=(__typeof__(&SysFreeString))automation("SysFreeString");
  if(native){native(value);return;}
  if (value) HeapFree(GetProcessHeap(), 0, (DWORD *)value - 1);
}
BIND(SysFreeString, 4);
static UINT WINAPI legacy_SysStringLen(BSTR value) {
  __typeof__(&SysStringLen) native=(__typeof__(&SysStringLen))automation("SysStringLen");
  if(native)return native(value);
  return value ? *((DWORD *)value - 1) / sizeof(OLECHAR) : 0;
}
BIND(SysStringLen, 4);

static BSTR WINAPI legacy_SysAllocStringByteLen(LPCSTR source,UINT length) {
  __typeof__(&SysAllocStringByteLen) native=(__typeof__(&SysAllocStringByteLen))automation("SysAllocStringByteLen");
  DWORD *block;BYTE *value;if(native)return native(source,length);
  if(length>UINT_MAX-sizeof(DWORD)-sizeof(OLECHAR))return NULL;
  block=HeapAlloc(GetProcessHeap(),0,sizeof(DWORD)+(SIZE_T)length+sizeof(OLECHAR));if(!block)return NULL;
  *block=length;value=(BYTE*)(block+1);if(source&&length)memcpy(value,source,length);value[length]=value[length+1]=0;return (BSTR)value;
}
BIND(SysAllocStringByteLen,8);
static UINT WINAPI legacy_SysStringByteLen(BSTR value) {
  __typeof__(&SysStringByteLen) native=(__typeof__(&SysStringByteLen))automation("SysStringByteLen");
  if(native)return native(value);return value?*((DWORD*)value-1):0;
}
BIND(SysStringByteLen,4);

static BOOL scalar_variant(VARTYPE type) {
  switch (type) {
    case VT_EMPTY: case VT_NULL: case VT_I1: case VT_UI1: case VT_I2: case VT_UI2:
    case VT_I4: case VT_UI4: case VT_I8: case VT_UI8: case VT_INT: case VT_UINT:
    case VT_R4: case VT_R8: case VT_CY: case VT_DATE: case VT_ERROR: case VT_BOOL:
    case VT_DECIMAL: return TRUE;
    default: return FALSE;
  }
}
static HRESULT WINAPI legacy_VariantClear(VARIANTARG *value) {
  __typeof__(&VariantClear) native=(__typeof__(&VariantClear))automation("VariantClear");
  if(native)return native(value);
  if (!value) return E_INVALIDARG;
  if (value->vt == VT_BSTR) legacy_SysFreeString(value->bstrVal);
  else if (!scalar_variant(value->vt)) return DISP_E_BADVARTYPE;
  value->vt = VT_EMPTY;
  return S_OK;
}
BIND(VariantClear, 4);
static HRESULT WINAPI legacy_VariantCopy(VARIANTARG *target, VARIANTARG *source) {
  __typeof__(&VariantCopy) native=(__typeof__(&VariantCopy))automation("VariantCopy");
  if(native)return native(target, source);
  VARIANTARG copy;
  HRESULT result;
  if (!target || !source) return E_INVALIDARG;
  if (target == source) return S_OK;
  if (!scalar_variant(source->vt) && source->vt != VT_BSTR) return DISP_E_BADVARTYPE;
  copy = *source;
  if (source->vt == VT_BSTR && source->bstrVal) {
    copy.bstrVal = legacy_SysAllocStringByteLen((LPCSTR)source->bstrVal, legacy_SysStringByteLen(source->bstrVal));
    if (!copy.bstrVal) return E_OUTOFMEMORY;
  }
  result = legacy_VariantClear(target);
  if (FAILED(result)) {
    if (copy.vt == VT_BSTR) legacy_SysFreeString(copy.bstrVal);
    return result;
  }
  *target = copy;
  return S_OK;
}
BIND(VariantCopy, 8);

/* libgcc also uses a direct call rather than the SDK import pointer. */
BOOL WINAPI IsProcessorFeaturePresent(DWORD feature) {
  return legacy_IsProcessorFeaturePresent(feature);
}
