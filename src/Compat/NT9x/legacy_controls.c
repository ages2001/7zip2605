/* Unicode message adapters for the Windows 95 RTM common controls.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
extern BOOL LegacyIsListView(HWND);
static HMODULE common_module(void){HMODULE module=GetModuleHandleA("comctl32.dll");return module?module:LoadLibraryA("comctl32.dll");}
#define BIND(name, bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes) = legacy_##name
/* Unicode toolbar messages were added in common-controls 4.71.
   NT 4.0 can export DllGetVersion while still shipping version 4.70. */
static BOOL unicode_toolbar(void) {
  typedef struct { DWORD size, major, minor, build, platform; } Version;
  typedef HRESULT (CALLBACK *Fn)(Version *);
  Fn fn=(Fn)GetProcAddress(common_module(),"DllGetVersion");
  Version v={sizeof(v),0,0,0,0};
  return fn && SUCCEEDED(fn(&v)) && (v.major>4 || (v.major==4 && v.minor>=71));
}
static char *ansi_text(LPCWSTR text) {
  int size; char *buffer;
  if (!text || text == LPSTR_TEXTCALLBACKW) return (char *)text;
  size = WideCharToMultiByte(CP_ACP,0,text,-1,NULL,0,NULL,NULL);
  if (!size) return NULL;
  buffer = HeapAlloc(GetProcessHeap(),0,size);
  if (buffer) WideCharToMultiByte(CP_ACP,0,text,-1,buffer,size,NULL,NULL);
  return buffer;
}
static void free_text(char *text) {
  if (text && text != LPSTR_TEXTCALLBACKA) HeapFree(GetProcessHeap(),0,text);
}
static LRESULT WINAPI legacy_SendMessageA(HWND window,UINT message,WPARAM wp,LPARAM lp) {
  typedef LRESULT (WINAPI *Fn)(HWND,UINT,WPARAM,LPARAM);
  Fn send = (Fn)GetProcAddress(GetModuleHandleA("user32.dll"),"SendMessageA");
  LRESULT result;
  if (LegacyIsListView(window)) return send(window,message,wp,lp);
  { char class_name[32];
    if (GetClassNameA(window,class_name,sizeof(class_name)) &&
        !lstrcmpiA(class_name,TOOLBARCLASSNAMEA) &&
        !GetProcAddress(common_module(),"CreateToolbarEx"))
      return send(window,message,wp,lp);
  }
  if (message == CBEM_INSERTITEMW || message == CBEM_GETEDITCONTROL) {
    char class_name[32];
    if (GetClassNameA(window,class_name,sizeof(class_name)) && !lstrcmpiA(class_name,"COMBOBOX")) {
      if (message == CBEM_GETEDITCONTROL) {
        HWND child=GetWindow(window,GW_CHILD);
        while(child) {
          if(GetClassNameA(child,class_name,sizeof(class_name))&&!lstrcmpiA(class_name,"EDIT"))return (LRESULT)child;
          child=GetWindow(child,GW_HWNDNEXT);
        }
        return 0;
      }
      { const COMBOBOXEXITEMW *item=(const COMBOBOXEXITEMW *)lp;
        char *text=ansi_text(item->pszText);
        if(item->pszText&&!text)return CB_ERRSPACE;
        result=send(window,CB_INSERTSTRING,item->iItem,(LPARAM)text);
        free_text(text);
        if(result>=0&&(item->mask&CBEIF_LPARAM))send(window,CB_SETITEMDATA,result,item->lParam);
        return result;
      }
    }
  }
  if (!(GetVersion() & 0x80000000UL) &&
      GetProcAddress(common_module(),"InitCommonControlsEx") &&
      (message != TB_ADDBUTTONSW || unicode_toolbar()))
    return send(window,message,wp,lp);
  if (message == LVM_INSERTITEMW || message == LVM_SETITEMW) {
    const LVITEMW *wide = (const LVITEMW *)lp;
    LVITEMA item;
    char *text = NULL;
    /* Only copy the fields present on Windows 95, not later SDK extensions. */
    ZeroMemory(&item,sizeof(item));
    item.mask=wide->mask; item.iItem=wide->iItem; item.iSubItem=wide->iSubItem;
    item.state=wide->state; item.stateMask=wide->stateMask;
    item.iImage=wide->iImage; item.lParam=wide->lParam;
    if (wide->mask & LVIF_TEXT) {
      text=ansi_text(wide->pszText); item.pszText=text; item.cchTextMax=wide->cchTextMax;
      if (wide->pszText && !text) return message==LVM_INSERTITEMW ? -1 : FALSE;
    }
    result=send(window,message==LVM_INSERTITEMW?LVM_INSERTITEMA:LVM_SETITEMA,wp,(LPARAM)&item);
    free_text(text); return result;
  }
  if (message == LVM_INSERTCOLUMNW || message == LVM_SETCOLUMNW) {
    const LVCOLUMNW *wide=(const LVCOLUMNW *)lp;
    LVCOLUMNA column; char *text=NULL;
    ZeroMemory(&column,sizeof(column));
    column.mask=wide->mask;column.fmt=wide->fmt;column.cx=wide->cx;column.iSubItem=wide->iSubItem;
    if (wide->mask & LVCF_TEXT) {
      text=ansi_text(wide->pszText);column.pszText=text;
      if(wide->pszText&&!text)return message==LVM_INSERTCOLUMNW?-1:FALSE;
    }
    result=send(window,message==LVM_INSERTCOLUMNW?LVM_INSERTCOLUMNA:LVM_SETCOLUMNA,wp,(LPARAM)&column);
    free_text(text);return result;
  }
  if (message == SB_SETTEXTW && !(wp & SBT_OWNERDRAW)) {
    char *text=ansi_text((LPCWSTR)lp);
    if(lp&&!text)return FALSE;
    result=send(window,SB_SETTEXTA,wp,(LPARAM)text);free_text(text);return result;
  }
  if (message == TB_ADDBUTTONSW) {
    UINT i; const TBBUTTON *wide=(const TBBUTTON *)lp;
    for(i=0;i<(UINT)wp;++i) {
      TBBUTTON button=wide[i];char *text=NULL;
      /* Win32s heap pointers can have bit 31 set. Classify the pointer as
         unsigned; a signed comparison silently drops the button label. */
      if((UINT_PTR)button.iString>65535 && button.iString != -1) {
        text=ansi_text((LPCWSTR)button.iString);
        if(!text)return FALSE;
        /* Old toolbar controls need a string-pool index, not a string pointer. */
        { int length=lstrlenA(text);char *pool=HeapAlloc(GetProcessHeap(),0,length+2);
          if(!pool){free_text(text);return FALSE;}
          CopyMemory(pool,text,length+1);pool[length+1]=0;
          button.iString=send(window,TB_ADDSTRINGA,0,(LPARAM)pool);
          HeapFree(GetProcessHeap(),0,pool);free_text(text);
          if(button.iString<0)return FALSE;
        }
      }
      if(!send(window,TB_ADDBUTTONSA,1,(LPARAM)&button))return FALSE;
    }
    return TRUE;
  }
  return send(window,message,wp,lp);
}
BIND(SendMessageA,16);
static HWND WINAPI legacy_CreateStatusWindowA(LONG style,LPCSTR text,HWND parent,UINT id) {
  typedef HWND (WINAPI *Fn)(LONG,LPCSTR,HWND,UINT);
  HMODULE module=common_module();
  Fn fn=(Fn)GetProcAddress(module,"CreateStatusWindowA");
  if(!fn)fn=(Fn)GetProcAddress(module,"CreateStatusWindow");
  if(!fn){SetLastError(ERROR_PROC_NOT_FOUND);return NULL;}
  return fn(style,text,parent,id);
}
BIND(CreateStatusWindowA,16);
static HWND WINAPI legacy_CreateStatusWindowW(LONG style,LPCWSTR text,HWND parent,UINT id) {
  typedef HWND (WINAPI *FnW)(LONG,LPCWSTR,HWND,UINT);
  FnW wide=(FnW)GetProcAddress(common_module(),"CreateStatusWindowW");
  char *ansi;HWND window;
  if(!(GetVersion()&0x80000000UL)&&wide)return wide(style,text,parent,id);
  ansi=ansi_text(text);if(text&&!ansi)return NULL;
  window=CreateStatusWindowA(style,ansi,parent,id);free_text(ansi);return window;
}
BIND(CreateStatusWindowW,16);
