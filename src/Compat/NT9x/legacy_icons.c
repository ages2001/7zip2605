/* Icon extraction for NT 3.1, preserving caller-owned icon handles.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#define BIND(name,bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes)=legacy_##name
extern HICON LegacySizedIcon(HICON source,int width,int height);
#include "legacy_icon_resources.h"
/* NT 3.1 exposes ExtractIconW, but not ExtractIconEx. Keep separate owned
   handles for both requested sizes, including the count-only query. */
static UINT WINAPI legacy_ExtractIconExW(LPCWSTR path,int index,HICON *large,HICON *small,UINT count) {
  typedef UINT (WINAPI *FnW)(LPCWSTR,int,HICON*,HICON*,UINT);
  typedef UINT (WINAPI *FnA)(LPCSTR,int,HICON*,HICON*,UINT);
  typedef HICON (WINAPI *OldFn)(HINSTANCE,LPCWSTR,UINT);
  typedef HICON (WINAPI *OldFnA)(HINSTANCE,LPCSTR,UINT);
  HMODULE shell=GetModuleHandleA("shell32.dll");
  DWORD version=GetVersion();BOOL win32s=(version&0x80000000UL)&&(version&255)==3;
  FnW wide;FnA ansi;OldFn old;OldFnA oldA=NULL;UINT i,done=0;char buffer[MAX_PATH];BOOL substituted=FALSE;
  if(!shell)shell=LoadLibraryA("shell32.dll");
  wide=shell?(FnW)GetProcAddress(shell,"ExtractIconExW"):NULL;
  ansi=shell?(FnA)GetProcAddress(shell,"ExtractIconExA"):NULL;
  /* NT3.51 exports an incomplete ExtractIconExW. Use the proven old
     extraction path for all NT3 releases, as with the shell PIDL adapter. */
  if((version&255)==3)wide=NULL,ansi=NULL;
  if(wide&&!(GetVersion()&0x80000000UL))return wide(path,index,large,small,count);
  if(ansi){
    for(i=0;i<count;i++){if(large)large[i]=NULL;if(small)small[i]=NULL;}
    if(!WideCharToMultiByte(CP_ACP,0,path,-1,buffer,sizeof(buffer),NULL,&substituted))return 0;
    if(substituted){SetLastError(ERROR_NO_UNICODE_TRANSLATION);return 0;}
    return ansi(buffer,index,large,small,count);
  }
  old=shell?(OldFn)GetProcAddress(shell,"ExtractIconW"):NULL;
  if(win32s){
    old=NULL;oldA=shell?(OldFnA)GetProcAddress(shell,"ExtractIconA"):NULL;
    if(!path||!WideCharToMultiByte(CP_ACP,0,path,-1,buffer,sizeof(buffer),NULL,&substituted))return 0;
    if(substituted){SetLastError(ERROR_NO_UNICODE_TRANSLATION);return 0;}
  }
  if((!old&&!oldA)||!path){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
  if(index==-1&&!large&&!small)return (UINT)(UINT_PTR)(oldA?oldA(GetModuleHandleA(NULL),buffer,(UINT)-1):old(GetModuleHandleA(NULL),path,(UINT)-1));
  if(!count||(!large&&!small))return 0;
  for(i=0;i<count;i++){
    HICON icon=NULL,big=NULL,little=NULL;
    if(large)large[i]=NULL;if(small)small[i]=NULL;
    if(index<0){
      HMODULE resource;HRSRC group,bitmap;const BYTE *data;DWORD size;UINT id;
      if(i||index < -65535)break;
      if(win32s){
        UINT ordinal;
        if(!iconResourceOrdinal(buffer,(UINT)-index,&ordinal))break;
        icon=oldA(GetModuleHandleA(NULL),buffer,ordinal);
      }else{
      resource=LoadLibraryExW(path,NULL,LOAD_LIBRARY_AS_DATAFILE);if(!resource)break;
      group=FindResourceW(resource,MAKEINTRESOURCEW(-index),MAKEINTRESOURCEW(14));
      size=group?SizeofResource(resource,group):0;
      data=group?(const BYTE*)LockResource(LoadResource(resource,group)):NULL;
      if(data&&size>=20&&*(const WORD*)data==0&&*(const WORD*)(data+2)==1&&*(const WORD*)(data+4)>0&&
          (DWORD)*(const WORD*)(data+4)<=(size-6)/14){
        id=*(const WORD*)(data+18);bitmap=FindResourceW(resource,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(3));
        if(bitmap){BYTE *bits=(BYTE*)LockResource(LoadResource(resource,bitmap));DWORD bytes=SizeofResource(resource,bitmap);if(bits&&bytes)icon=CreateIconFromResource(bits,bytes,TRUE,0x00030000);}
      }
      FreeLibrary(resource);
      }
    }else icon=oldA?oldA(GetModuleHandleA(NULL),buffer,(UINT)index+i):old(GetModuleHandleA(NULL),path,(UINT)index+i);
    if((UINT_PTR)icon<=1)break;
    if(large)big=LegacySizedIcon(icon,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON));
    if(small)little=LegacySizedIcon(icon,16,16);
    DestroyIcon(icon);
    if((large&&!big)||(small&&!little)){if(big)DestroyIcon(big);if(little)DestroyIcon(little);break;}
    if(large)large[i]=big;if(small)small[i]=little;done++;
  }
  return done;
}
BIND(ExtractIconExW,20);
