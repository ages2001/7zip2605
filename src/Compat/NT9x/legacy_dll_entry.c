/* The Windows 95 loader rejects DLLs with a static TLS directory. This build
   has no static TLS variables; MinGW uses the directory only for callbacks.
   Dispatch those callbacks from the DLL entry point on every supported OS.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
extern const IMAGE_TLS_DIRECTORY _tls_used;
extern BOOL WINAPI DllMainCRTStartup(HINSTANCE,DWORD,LPVOID);
BOOL WINAPI LegacyDllEntry(HINSTANCE module,DWORD reason,LPVOID reserved)
{
  PIMAGE_TLS_CALLBACK *callback=(PIMAGE_TLS_CALLBACK*)_tls_used.AddressOfCallBacks;
  if(callback)for(;*callback;callback++)(*callback)(module,reason,reserved);
  return DllMainCRTStartup(module,reason,reserved);
}
