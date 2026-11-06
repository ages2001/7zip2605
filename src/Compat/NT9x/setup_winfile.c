/* Register the self-contained WinFile add-on on Win32 NT File Manager.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "setup_shortcuts.h"

DWORD Z7SetupWinFileW(const WCHAR *directory, BOOL remove)
{
  DWORD version=GetVersion();
  WCHAR ini[MAX_PATH],dll[MAX_PATH],existing[2*MAX_PATH],system[MAX_PATH];
  UINT n;
  if((version&0x80000000UL)||(version&255)>4)return ERROR_SUCCESS;
  if(!directory||!*directory)return ERROR_INVALID_PARAMETER;
  n=GetSystemDirectoryW(system,MAX_PATH);
  if(!n||n+13>=MAX_PATH)return ERROR_BAD_PATHNAME;
  lstrcatW(system,L"\\WINFILE.EXE");
  if(GetFileAttributesW(system)==INVALID_FILE_ATTRIBUTES)return ERROR_SUCCESS;
  n=GetWindowsDirectoryW(ini,MAX_PATH);
  if(!n||n+13>=MAX_PATH)return ERROR_BAD_PATHNAME;
  lstrcatW(ini,L"\\WINFILE.INI");
  n=lstrlenW(directory);
  if(n+12>=MAX_PATH)return ERROR_FILENAME_EXCED_RANGE;
  lstrcpyW(dll,directory);
  if(dll[n-1]!='\\'&&dll[n-1]!='/')lstrcatW(dll,L"\\");
  lstrcatW(dll,L"7zWF.dll");
  GetPrivateProfileStringW(L"AddOns",L"7-Zip",L"",existing,2*MAX_PATH,ini);
  if(remove){
    /* Preserve a user-edited add-on registration. */
    if(lstrcmpiW(existing,dll))return ERROR_SUCCESS;
  }else{
    if(GetFileAttributesW(dll)==INVALID_FILE_ATTRIBUTES)return ERROR_FILE_NOT_FOUND;
    if(*existing&&lstrcmpiW(existing,dll))return ERROR_ALREADY_EXISTS;
  }
  SetLastError(ERROR_SUCCESS);
  if(!WritePrivateProfileStringW(L"AddOns",L"7-Zip",remove?NULL:dll,ini)){
    DWORD error=GetLastError();return error?error:ERROR_WRITE_FAULT;
  }
  return ERROR_SUCCESS;
}
