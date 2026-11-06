/* System information adapters for Win32s.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later

   Win32s 1.30 does not implement GetLogicalDriveStringsA/W (error 120),
   while GetLogicalDrives and GetDriveTypeA work. The drive list is rebuilt
   from those functions. Phantom drive letters for which Windows 3.1 reports
   DRIVE_NO_ROOT_DIR (such as B: on a one-floppy machine) are omitted, as on
   later Windows versions. Other systems use the native functions. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define BIND(name,bytes) __typeof__(&name) w32s_sys_import_##name __asm__("__imp__" #name "@" #bytes)=w32s_##name
#define NATIVE(name) ((__typeof__(&name))GetProcAddress(GetModuleHandleA("kernel32.dll"),#name))

static BOOL isWin32s(void){DWORD v=GetVersion();return (v&0x80000000UL)&&(v&255)==3;}

/* Fills letters[] with present drive letters; returns their count. */
static unsigned presentDrives(char letters[26]){
  DWORD mask=GetLogicalDrives();unsigned i,n=0;
  for(i=0;i<26;i++)if(mask&(1UL<<i)){
    char root[4];UINT type;
    root[0]=(char)('A'+i);root[1]=':';root[2]='\\';root[3]=0;
    type=GetDriveTypeA(root);
    if(type!=DRIVE_NO_ROOT_DIR&&type!=DRIVE_UNKNOWN)letters[n++]=root[0];
  }
  return n;
}

static DWORD WINAPI w32s_GetLogicalDriveStringsA(DWORD capacity,LPSTR buffer){
  __typeof__(&GetLogicalDriveStringsA) native=NATIVE(GetLogicalDriveStringsA);
  char letters[26];unsigned n,i;DWORD need;
  if(!isWin32s()&&native)return native(capacity,buffer);
  n=presentDrives(letters);need=4*n+1;
  if(!buffer||capacity<need)return need;
  for(i=0;i<n;i++){buffer[4*i]=letters[i];buffer[4*i+1]=':';buffer[4*i+2]='\\';buffer[4*i+3]=0;}
  buffer[4*n]=0;
  return need-1;
}
BIND(GetLogicalDriveStringsA,8);

static DWORD WINAPI w32s_GetLogicalDriveStringsW(DWORD capacity,LPWSTR buffer){
  __typeof__(&GetLogicalDriveStringsW) native=NATIVE(GetLogicalDriveStringsW);
  char letters[26];unsigned n,i;DWORD need;
  if(!isWin32s()&&native)return native(capacity,buffer);
  n=presentDrives(letters);need=4*n+1;
  if(!buffer||capacity<need)return need;
  for(i=0;i<n;i++){buffer[4*i]=(WCHAR)(BYTE)letters[i];buffer[4*i+1]=L':';buffer[4*i+2]=L'\\';buffer[4*i+3]=0;}
  buffer[4*n]=0;
  return need-1;
}
BIND(GetLogicalDriveStringsW,8);
