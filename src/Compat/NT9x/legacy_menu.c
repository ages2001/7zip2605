/* Menu APIs for NT 3.1. Native implementations are preferred when present.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0500
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <stddef.h>
#define BIND(name, bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes) = legacy_##name
#define NATIVE(name) ((__typeof__(&name))GetProcAddress(GetModuleHandleA("user32.dll"), #name))
/* Application menus are owned by the UI thread, as required by USER. Extra
   fields are kept separately because the old MENU structure has no item data. */
typedef struct Extra { struct Extra *next; HMENU menu; UINT pos, id, type; ULONG_PTR data; HBITMAP checked, unchecked; BOOL has_id; } Extra;
static Extra *extras;
static Extra *extra(HMENU menu,UINT pos,BOOL create) {
  Extra *e;for(e=extras;e;e=e->next)if(e->menu==menu&&e->pos==pos)return e;
  if(!create)return NULL;
  e=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*e));
  if(!e){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
  e->menu=menu;e->pos=pos;e->next=extras;extras=e;return e;
}
static void shift(HMENU menu,UINT pos,int delta) {
  Extra **link=&extras,*e;
  while((e=*link)!=NULL) {
    if(e->menu==menu&&delta<0&&e->pos==pos){*link=e->next;HeapFree(GetProcessHeap(),0,e);continue;}
    if(e->menu==menu&&e->pos>=pos)e->pos+=delta;
    link=&e->next;
  }
}
static void forget(HMENU menu) {
  Extra **link=&extras,*e;int n=GetMenuItemCount(menu),i;
  for(i=0;i<n;i++){HMENU sub=GetSubMenu(menu,i);if(sub)forget(sub);}
  while((e=*link)!=NULL){if(e->menu==menu){*link=e->next;HeapFree(GetProcessHeap(),0,e);}else link=&e->next;}
}
static BOOL locate(HMENU *menu,UINT *item,BOOL position,unsigned depth) {
  int n=GetMenuItemCount(*menu),i;
  if(n<0||depth>64)return FALSE;
  if(position)return *item<(UINT)n;
  for(i=0;i<n;i++) {
    Extra *e=extra(*menu,i,FALSE);HMENU sub=GetSubMenu(*menu,i);UINT id=*item;
    if((e&&e->has_id&&e->id==id)||(!sub&&GetMenuItemID(*menu,i)==id)){*item=i;return TRUE;}
    if(sub&&locate(&sub,&id,FALSE,depth+1)){*menu=sub;*item=id;return TRUE;}
  }
  return FALSE;
}
static BOOL valid(const MENUITEMINFOA *info) {
  if(!info||(info->cbSize!=sizeof(*info)&&info->cbSize!=offsetof(MENUITEMINFOA,hbmpItem))){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  /* The application uses the original Win95 menu fields. Never claim bitmap
     item support through a newer field that the old USER cannot represent. */
  if(info->fMask & ~(MIIM_TYPE|MIIM_FTYPE|MIIM_STRING|MIIM_STATE|MIIM_ID|MIIM_SUBMENU|MIIM_CHECKMARKS|MIIM_DATA)){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
  return TRUE;
}
/* NT 3.1 GetMenuString does not support a NULL/zero length query. */
static UINT string_length(HMENU menu,UINT item,BOOL wide) {
  UINT cap=128,n;void *buffer;
  for(;;){
    buffer=HeapAlloc(GetProcessHeap(),0,cap*(wide?sizeof(WCHAR):1));
    if(!buffer){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return (UINT)-1;}
    n=wide?GetMenuStringW(menu,item,buffer,cap,MF_BYPOSITION):GetMenuStringA(menu,item,buffer,cap,MF_BYPOSITION);
    HeapFree(GetProcessHeap(),0,buffer);
    if(n<cap-1)return n;
    if(cap>=65536){SetLastError(ERROR_INSUFFICIENT_BUFFER);return (UINT)-1;}
    cap*=2;
  }
}
static BOOL get_item(HMENU menu,UINT item,BOOL position,MENUITEMINFOA *info,BOOL wide) {
  UINT state,type;Extra *e;
  if(!valid(info))return FALSE;
  if(!locate(&menu,&item,position,0)){SetLastError(ERROR_MENU_ITEM_NOT_FOUND);return FALSE;}
  state=GetMenuState(menu,item,MF_BYPOSITION);e=extra(menu,item,FALSE);
  type=state&(MFT_BITMAP|MFT_MENUBARBREAK|MFT_MENUBREAK|MFT_OWNERDRAW|MFT_SEPARATOR|MFT_RIGHTJUSTIFY);
  if(e)type|=e->type&MFT_RADIOCHECK;
  if(info->fMask&(MIIM_TYPE|MIIM_FTYPE))info->fType=type;
  if(info->fMask&MIIM_STATE)info->fState=state&(MFS_GRAYED|MFS_CHECKED|MFS_HILITE);
  if(info->fMask&MIIM_ID)info->wID=(e&&e->has_id)?e->id:GetMenuItemID(menu,item);
  if(info->fMask&MIIM_SUBMENU)info->hSubMenu=GetSubMenu(menu,item);
  if(info->fMask&MIIM_DATA)info->dwItemData=e?e->data:0;
  if(info->fMask&MIIM_CHECKMARKS){info->hbmpChecked=e?e->checked:NULL;info->hbmpUnchecked=e?e->unchecked:NULL;}
  if(info->fMask&(MIIM_TYPE|MIIM_STRING)) {
    if(type&(MFT_OWNERDRAW|MFT_BITMAP|MFT_SEPARATOR)){info->cch=0;info->dwTypeData=(LPSTR)((e&&(type&MFT_OWNERDRAW))?e->data:0);}
    else if(!info->dwTypeData||!info->cch){info->cch=string_length(menu,item,wide);if(info->cch==(UINT)-1)return FALSE;}
    else if(wide)info->cch=GetMenuStringW(menu,item,(LPWSTR)info->dwTypeData,info->dwTypeData?info->cch:0,MF_BYPOSITION);
    else info->cch=GetMenuStringA(menu,item,info->dwTypeData,info->dwTypeData?info->cch:0,MF_BYPOSITION);
  }
  return TRUE;
}
static BOOL put_item(HMENU menu,UINT item,BOOL position,const MENUITEMINFOA *info,BOOL wide,BOOL insert) {
  MENUITEMINFOA old;UINT flags,id;UINT_PTR native_id;Extra *e;void *text=NULL;BOOL ok;int count;
  if(!valid(info))return FALSE;
  if(insert){count=GetMenuItemCount(menu);if(count<0)return FALSE;if(position){if(item>(UINT)count)item=count;}else if(!locate(&menu,&item,FALSE,0))item=GetMenuItemCount(menu);}
  else if(!locate(&menu,&item,position,0)){SetLastError(ERROR_MENU_ITEM_NOT_FOUND);return FALSE;}
  ZeroMemory(&old,sizeof(old));old.cbSize=sizeof(old);
  old.fMask=MIIM_TYPE|MIIM_STATE|MIIM_ID|MIIM_SUBMENU|MIIM_DATA|MIIM_CHECKMARKS;
  if(!insert&&!get_item(menu,item,TRUE,&old,wide))return FALSE;
  if(info->fMask&(MIIM_TYPE|MIIM_FTYPE))old.fType=info->fType;
  if(info->fMask&MIIM_STATE)old.fState=info->fState;
  if(info->fMask&MIIM_ID)old.wID=info->wID;
  if(info->fMask&MIIM_SUBMENU)old.hSubMenu=info->hSubMenu;
  if(info->fMask&MIIM_DATA)old.dwItemData=info->dwItemData;
  if(info->fMask&MIIM_CHECKMARKS){old.hbmpChecked=info->hbmpChecked;old.hbmpUnchecked=info->hbmpUnchecked;}
  if(info->fMask&(MIIM_TYPE|MIIM_STRING))old.dwTypeData=info->dwTypeData;
  else if(!insert&&!(old.fType&(MFT_OWNERDRAW|MFT_BITMAP|MFT_SEPARATOR))) {
    UINT capacity=old.cch+1;
    text=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,capacity*(wide?sizeof(WCHAR):1));
    if(!text){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    if(wide)GetMenuStringW(menu,item,text,capacity,MF_BYPOSITION);else GetMenuStringA(menu,item,text,capacity,MF_BYPOSITION);
    old.dwTypeData=text;
  }
  /* Allocate metadata before mutating the native menu. */
  if(insert){e=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*e));if(e){e->menu=menu;e->pos=item;}}
  else e=extra(menu,item,TRUE);
  if(!e){if(text)HeapFree(GetProcessHeap(),0,text);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
  flags=MF_BYPOSITION|(old.fType&~MFT_RADIOCHECK)|old.fState;id=old.wID;native_id=id;
  if(old.hSubMenu){flags|=MF_POPUP;native_id=(UINT_PTR)old.hSubMenu;}
  if(old.fType&MFT_OWNERDRAW)old.dwTypeData=(LPSTR)old.dwItemData;
  if(insert)ok=wide?InsertMenuW(menu,item,flags,native_id,(LPCWSTR)old.dwTypeData):InsertMenuA(menu,item,flags,native_id,old.dwTypeData);
  else ok=wide?ModifyMenuW(menu,item,flags,native_id,(LPCWSTR)old.dwTypeData):ModifyMenuA(menu,item,flags,native_id,old.dwTypeData);
  if(text)HeapFree(GetProcessHeap(),0,text);
  if(!ok){if(insert)HeapFree(GetProcessHeap(),0,e);return FALSE;}
  if(insert){shift(menu,item,1);e->next=extras;extras=e;}
  e->id=id;e->has_id=TRUE;e->type=old.fType;e->data=old.dwItemData;e->checked=old.hbmpChecked;e->unchecked=old.hbmpUnchecked;
  return SetMenuItemBitmaps(menu,item,MF_BYPOSITION,e->unchecked,e->checked);
}
#define MENU_APIS(suffix,wide) \
static BOOL WINAPI legacy_GetMenuItemInfo##suffix(HMENU m,UINT i,BOOL p,MENUITEMINFO##suffix *v){__typeof__(&GetMenuItemInfo##suffix) fn=NATIVE(GetMenuItemInfo##suffix);return fn?fn(m,i,p,v):get_item(m,i,p,(MENUITEMINFOA*)v,wide);} \
BIND(GetMenuItemInfo##suffix,16); \
static BOOL WINAPI legacy_SetMenuItemInfo##suffix(HMENU m,UINT i,BOOL p,const MENUITEMINFO##suffix *v){__typeof__(&SetMenuItemInfo##suffix) fn=NATIVE(SetMenuItemInfo##suffix);return fn?fn(m,i,p,v):put_item(m,i,p,(const MENUITEMINFOA*)v,wide,FALSE);} \
BIND(SetMenuItemInfo##suffix,16); \
static BOOL WINAPI legacy_InsertMenuItem##suffix(HMENU m,UINT i,BOOL p,const MENUITEMINFO##suffix *v){__typeof__(&InsertMenuItem##suffix) fn=NATIVE(InsertMenuItem##suffix);return fn?fn(m,i,p,v):put_item(m,i,p,(const MENUITEMINFOA*)v,wide,TRUE);} \
BIND(InsertMenuItem##suffix,16)
MENU_APIS(A,FALSE);
MENU_APIS(W,TRUE);
static BOOL WINAPI legacy_RemoveMenu(HMENU menu,UINT item,UINT flags) {
  HMENU target=menu;UINT pos=item;BOOL found=locate(&target,&pos,(flags&MF_BYPOSITION)!=0,0);
  BOOL ok=NATIVE(RemoveMenu)(menu,item,flags);if(ok&&found)shift(target,pos,-1);return ok;
}
BIND(RemoveMenu,12);
static BOOL WINAPI legacy_DestroyMenu(HMENU menu) {
  /* Remove metadata while submenus can still be enumerated. */
  if(!IsMenu(menu))return FALSE;
  forget(menu);return NATIVE(DestroyMenu)(menu);
}
BIND(DestroyMenu,4);
static BOOL WINAPI legacy_CheckMenuRadioItem(HMENU menu,UINT first,UINT last,UINT selected,UINT flags) {
  __typeof__(&CheckMenuRadioItem) fn=NATIVE(CheckMenuRadioItem);UINT i;BOOL found=FALSE;
  static HBITMAP bullet;static const WORD bits[8]={0,0,0x1800,0x3c00,0x3c00,0x1800,0,0};
  if(fn)return fn(menu,first,last,selected,flags);
  if(first>last||(flags&~MF_BYPOSITION)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  if(!bullet){bullet=CreateBitmap(8,8,1,1,bits);if(!bullet)return FALSE;}
  for(i=first;;i++) {
    HMENU target=menu;UINT pos=i;
    if(locate(&target,&pos,(flags&MF_BYPOSITION)!=0,0)) {
      Extra *e=extra(target,pos,TRUE);if(!e)return FALSE;
      if(i==selected){e->type|=MFT_RADIOCHECK;e->checked=bullet;found=TRUE;}else e->type&=~MFT_RADIOCHECK;
      if(CheckMenuItem(target,pos,MF_BYPOSITION|(i==selected?MF_CHECKED:MF_UNCHECKED))==(DWORD)-1)return FALSE;
      if(!SetMenuItemBitmaps(target,pos,MF_BYPOSITION,e->unchecked,e->checked))return FALSE;
    }
    if(i==last)break;
  }
  return found;
}
BIND(CheckMenuRadioItem,20);
static HWND WINAPI legacy_ChildWindowFromPointEx(HWND parent,POINT pt,UINT flags) {
  __typeof__(&ChildWindowFromPointEx) fn=NATIVE(ChildWindowFromPointEx);RECT r;POINT screen=pt;HWND child;
  if(fn)return fn(parent,pt,flags);
  if(flags&~(CWP_SKIPINVISIBLE|CWP_SKIPDISABLED|CWP_SKIPTRANSPARENT)){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
  if(!GetClientRect(parent,&r)||!PtInRect(&r,pt))return NULL;
  ClientToScreen(parent,&screen);
  for(child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
    LONG style=GetWindowLongA(child,GWL_STYLE),exstyle=GetWindowLongA(child,GWL_EXSTYLE);
    if((flags&CWP_SKIPINVISIBLE)&&!(style&WS_VISIBLE))continue;
    if((flags&CWP_SKIPDISABLED)&&(style&WS_DISABLED))continue;
    if((flags&CWP_SKIPTRANSPARENT)&&(exstyle&WS_EX_TRANSPARENT))continue;
    if(GetWindowRect(child,&r)&&PtInRect(&r,screen))return child;
  }
  return parent;
}
BIND(ChildWindowFromPointEx,16);
static BOOL WINAPI legacy_TrackPopupMenuEx(HMENU menu,UINT flags,int x,int y,HWND owner,LPTPMPARAMS params) {
  __typeof__(&TrackPopupMenuEx) fn=NATIVE(TrackPopupMenuEx);
  if(fn)return fn(menu,flags,x,y,owner,params);
  if(params&&params->cbSize!=sizeof(*params)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  return TrackPopupMenu(menu,flags,x,y,0,owner,params?&params->rcExclude:NULL);
}
BIND(TrackPopupMenuEx,24);
/* NT 3.1 USER understands MENU templates, but not MENUEX resources. Preserve
   submenu IDs in the same metadata used by Get/SetMenuItemInfo. */
static HMENU menuExLevel(const BYTE *base,const BYTE **cursor,const BYTE *end,BOOL popup,unsigned depth){HMENU menu;BOOL finished=FALSE;if(depth>64)return NULL;menu=popup?CreatePopupMenu():CreateMenu();if(!menu)return NULL;
 while(!finished){const BYTE *p=*cursor;DWORD type,state,id;WORD info;const WCHAR *text;HMENU sub=NULL;UINT pos;Extra *e;ULONG_PTR offset=(ULONG_PTR)(p-base);offset=(offset+3)&~(ULONG_PTR)3;p=base+offset;if(p>end||end-p<14)goto fail;type=*(const DWORD*)p;state=*(const DWORD*)(p+4);id=*(const DWORD*)(p+8);info=*(const WORD*)(p+12);p+=14;text=(const WCHAR*)p;while(p<end&&end-p>=2&&*(const WCHAR*)p)p+=2;if(end-p<2)goto fail;p+=2;if(info&1){offset=(ULONG_PTR)(p-base);offset=(offset+3)&~(ULONG_PTR)3;p=base+offset;if(p>end||end-p<4)goto fail;p+=4;*cursor=p;sub=menuExLevel(base,cursor,end,TRUE,depth+1);if(!sub)goto fail;p=*cursor;}pos=GetMenuItemCount(menu);if(!AppendMenuW(menu,type|state|(sub?MF_POPUP:0),sub?(UINT_PTR)sub:id,text)){if(sub)legacy_DestroyMenu(sub);goto fail;}e=extra(menu,pos,TRUE);if(!e)goto fail;e->id=id;e->has_id=TRUE;e->type=type;*cursor=p;finished=(info&0x80)!=0;
 }return menu;
fail:legacy_DestroyMenu(menu);SetLastError(ERROR_BAD_FORMAT);return NULL;}
static HMENU loadMenuEx(HINSTANCE instance,HRSRC resource){HGLOBAL memory;const BYTE *base,*p;DWORD size;if(!resource)return NULL;size=SizeofResource(instance,resource);memory=LoadResource(instance,resource);base=memory?LockResource(memory):NULL;if(!base||size<8||*(const WORD*)base!=1)return NULL;if(*(const WORD*)(base+2)>size-4)return NULL;p=base+4+*(const WORD*)(base+2);return menuExLevel(base,&p,base+size,FALSE,0);}
static HMENU WINAPI legacy_LoadMenuA(HINSTANCE instance,LPCSTR name){__typeof__(&LoadMenuA) fn=NATIVE(LoadMenuA);HMENU menu;DWORD v=GetVersion();if(!(v&0x80000000UL)&&LOBYTE(LOWORD(v))<4){menu=loadMenuEx(instance,FindResourceA(instance,name,RT_MENU));if(menu)return menu;}menu=fn?fn(instance,name):NULL;return menu?menu:loadMenuEx(instance,FindResourceA(instance,name,RT_MENU));}
BIND(LoadMenuA,8);
static HMENU WINAPI legacy_LoadMenuW(HINSTANCE instance,LPCWSTR name){__typeof__(&LoadMenuW) fn=NATIVE(LoadMenuW);HMENU menu;DWORD v=GetVersion();if(!(v&0x80000000UL)&&LOBYTE(LOWORD(v))<4){menu=loadMenuEx(instance,FindResourceW(instance,name,(LPCWSTR)RT_MENU));if(menu)return menu;}menu=fn?fn(instance,name):NULL;return menu?menu:loadMenuEx(instance,FindResourceW(instance,name,(LPCWSTR)RT_MENU));}
BIND(LoadMenuW,8);
