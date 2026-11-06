/* Shortcut integration fixture. Never include this executable in release bin.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "setup_shortcuts.h"
void *memset(void *p, int c, unsigned n) { unsigned i; for(i=0;i<n;++i)((char*)p)[i]=(char)c; return p; }
static HANDLE report;
static void logLine(const char *s) { DWORD written; WriteFile(report,s,lstrlenA(s),&written,NULL); WriteFile(report,"\r\n",2,&written,NULL); FlushFileBuffers(report); }
static void check(BOOL success, const char *name) { if(!success) { logLine("FAIL"); logLine(name); CloseHandle(report); ExitProcess(1); } logLine(name); }
void entry(void)
{
  char module[MAX_PATH], number[40], installPath[MAX_PATH];
  DWORD error;
  GetModuleFileNameA(NULL,module,MAX_PATH);
  if (!lstrcmpiA(module,"C:\\Z7LINKT\\7ZFM.EXE")) {
    report=CreateFileA("C:\\Z7LINKT\\LAUNCHED.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
    logLine("PASS: shortcut target launched");
    GetCurrentDirectoryA(MAX_PATH,module);
    check(!lstrcmpiA(module,"C:\\Z7LINKT"),"PASS: working directory");
    CloseHandle(report);
    MessageBoxA(NULL,"Shortcut target and working directory verified.","7-Zip shortcut test",MB_OK);
    ExitProcess(0);
  }
  report=CreateFileA("C:\\SHORTCUT.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
  if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
  check(Z7SetupUsesProgman(VER_PLATFORM_WIN32_NT,3),"PASS: NT 3.xx selects Program Manager");
  check(Z7SetupUsesProgman(VER_PLATFORM_WIN32s,3),"PASS: Win32s selects Program Manager");
  check(Z7SetupPlatform(0x80000A03UL)==VER_PLATFORM_WIN32s,"PASS: Win3.1 version identifies Win32s");
  check(Z7SetupPlatform(0x80000004UL)==VER_PLATFORM_WIN32_WINDOWS,"PASS: Win95 version identifies Windows");
  check(Z7SetupPlatform(0x00000A03UL)==VER_PLATFORM_WIN32_NT,"PASS: NT3.1 version identifies NT");
  check(Z7SetupPlatform(0x00000004UL)==VER_PLATFORM_WIN32_NT,"PASS: NT4 version identifies NT");
  check(!Z7SetupUsesProgman(VER_PLATFORM_WIN32_NT,4),"PASS: NT 4 selects Explorer");
  check(!Z7SetupUsesProgman(VER_PLATFORM_WIN32_WINDOWS,4),"PASS: Windows 95 selects Explorer");
  check(!Z7SetupUsesProgman(VER_PLATFORM_WIN32_NT,5),"PASS: newer NT selects Explorer");
  check(Z7SetupCreateShortcuts("relative.exe","C:\\")==ERROR_BAD_PATHNAME,"PASS: relative target rejected");
  check(!Z7SetupDefaultPath(installPath, MAX_PATH),"PASS: default installation path");
  logLine(installPath);
  if (Z7SetupUsesProgman(Z7SetupPlatform(GetVersion()), LOBYTE(LOWORD(GetVersion()))))
    check(!lstrcmpA(installPath,"C:\\7-Zip"),"PASS: NT 3.xx / Win32s default is C:\\7-Zip");
  check(Z7SetupDefaultPath(installPath,2)==ERROR_INSUFFICIENT_BUFFER,"PASS: small path buffer rejected");
  CreateDirectoryA("C:\\Z7LINKT",NULL);
  check(CopyFileA(module,"C:\\Z7LINKT\\7ZFM.EXE",FALSE),"PASS: prepare launch probe");
  error=Z7SetupCreateShortcuts("C:\\Z7LINKT\\7ZFM.EXE","C:\\Z7LINKT");
  if(error) { wsprintfA(number,"Win32 error: %lu",error); logLine(number); }
  check(!error,"PASS: create shortcuts");
  error=Z7SetupCreateShortcuts("C:\\Z7LINKT\\7ZFM.EXE","C:\\Z7LINKT");
  if(error) { wsprintfA(number,"Win32 error: %lu",error); logLine(number); }
  check(!error,"PASS: repeat installation");
  logLine("PASS ALL creation checks; launch and reboot checks are separate.");
  CloseHandle(report);
  ExitProcess(0);
}
