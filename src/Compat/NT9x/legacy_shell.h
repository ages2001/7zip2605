/* Original NT 3.1 shell compatibility services.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_LEGACY_SHELL_H
#define Z7_LEGACY_SHELL_H
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#define SHELL_BIND(name, bytes) __typeof__(&name) shell_import_##name __asm__("__imp__" #name "@" #bytes) = name
#define SHELL_NATIVE(name) ((__typeof__(&name))Z7ShellNative(#name))
#define Z7_PATH 260
#define Z7_DESKTOP 1
#define Z7_COMPUTER 2
#define Z7_NETWORK 3
#define Z7_FILE 4
void *Z7ShellAlloc(SIZE_T);
void Z7ShellFree(void *);
FARPROC Z7ShellNative(const char *);
BOOL Z7ShellEqual(REFGUID,REFGUID);
BOOL Z7ShellWide(const char *,WCHAR *,UINT);
BOOL Z7ShellAnsi(const WCHAR *,char *,UINT);
BOOL Z7ShellJoin(const WCHAR *,const WCHAR *,WCHAR *);
const WCHAR *Z7ShellBase(const WCHAR *);
LPITEMIDLIST Z7ShellPidl(UINT,const WCHAR *);
UINT Z7ShellDecode(LPCITEMIDLIST,WCHAR *);
HRESULT Z7ShellFolder(UINT,const WCHAR *,REFIID,void **);
HRESULT Z7ShellMenu(HWND,UINT,LPCITEMIDLIST *,REFIID,void **);
DWORD Z7ShellAttributes(UINT,const WCHAR *);
#endif
