/* GUI API fallbacks for NT3 and the original Windows 95 desktop.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#define WINVER 0x0500
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
static HMODULE common_module(void){HMODULE module=GetModuleHandleA("comctl32.dll");return module?module:LoadLibraryA("comctl32.dll");}
#define BIND(name, bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes) = legacy_##name

/* NT 3.1 does not export the bounded A/W string-copy entry points. */
static LPSTR WINAPI legacy_lstrcpynA(LPSTR target,LPCSTR source,int count) {
  int i=0;if(!target||(!source&&count>0)){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
  if(count<=0)return target;
  while(i<count-1&&source[i]){target[i]=source[i];i++;}target[i]=0;return target;
}
BIND(lstrcpynA,12);
static LPWSTR WINAPI legacy_lstrcpynW(LPWSTR target,LPCWSTR source,int count) {
  int i=0;if(!target||(!source&&count>0)){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
  if(count<=0)return target;
  while(i<count-1&&source[i]){target[i]=source[i];i++;}target[i]=0;return target;
}
BIND(lstrcpynW,12);

extern BOOL LegacyRegisterProgress(void);
extern BOOL LegacyRegisterUpDown(void);
extern BOOL LegacyRegisterToolbar(void);
extern BOOL LegacyRegisterListView(void);
static void WINAPI legacy_InitCommonControls(void) {
  __typeof__(&InitCommonControls) fn=(__typeof__(&InitCommonControls))GetProcAddress(common_module(),"InitCommonControls");
  if(fn)fn();LegacyRegisterToolbar();LegacyRegisterListView();LegacyRegisterProgress();LegacyRegisterUpDown();
}
BIND(InitCommonControls,0);

static BOOL WINAPI legacy_InitCommonControlsEx(const INITCOMMONCONTROLSEX *info) {
  typedef BOOL (WINAPI *Fn)(const INITCOMMONCONTROLSEX *);
  HMODULE module=common_module();
  Fn fn=module?(Fn)GetProcAddress(module,"InitCommonControlsEx"):NULL;
  if(fn){BOOL ok=fn(info);if(ok){LegacyRegisterToolbar();LegacyRegisterListView();LegacyRegisterProgress();LegacyRegisterUpDown();}return ok;}
  if(!info||info->dwSize!=sizeof(*info)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  if(info->dwICC & ~ICC_WIN95_CLASSES){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
  InitCommonControls();return TRUE;
}
BIND(InitCommonControlsEx,4);
static HMONITOR WINAPI legacy_MonitorFromWindow(HWND window,DWORD flags) {
  typedef HMONITOR (WINAPI *Fn)(HWND,DWORD);
  Fn fn=(Fn)GetProcAddress(GetModuleHandleA("user32.dll"),"MonitorFromWindow");
  RECT rect;
  if(fn)return fn(window,flags);
  if(flags & (MONITOR_DEFAULTTOPRIMARY|MONITOR_DEFAULTTONEAREST))return (HMONITOR)1;
  if(GetWindowRect(window,&rect)&&rect.right>0&&rect.bottom>0&&rect.left<GetSystemMetrics(SM_CXSCREEN)&&rect.top<GetSystemMetrics(SM_CYSCREEN))return (HMONITOR)1;
  return NULL;
}
BIND(MonitorFromWindow,8);
static BOOL WINAPI legacy_GetMonitorInfoA(HMONITOR monitor,LPMONITORINFO info) {
  typedef BOOL (WINAPI *Fn)(HMONITOR,LPMONITORINFO);
  Fn fn=(Fn)GetProcAddress(GetModuleHandleA("user32.dll"),"GetMonitorInfoA");
  if(fn)return fn(monitor,info);
  if(monitor!=(HMONITOR)1||!info||(info->cbSize!=sizeof(MONITORINFO)&&info->cbSize!=sizeof(MONITORINFOEXA))){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  info->rcMonitor.left=info->rcMonitor.top=0;
  info->rcMonitor.right=GetSystemMetrics(SM_CXSCREEN);info->rcMonitor.bottom=GetSystemMetrics(SM_CYSCREEN);
  info->rcWork=info->rcMonitor;
  SystemParametersInfoA(SPI_GETWORKAREA,0,&info->rcWork,0);
  info->dwFlags=MONITORINFOF_PRIMARY;
  if(info->cbSize==sizeof(MONITORINFOEXA))lstrcpyA(((MONITORINFOEXA*)info)->szDevice,"DISPLAY");
  return TRUE;
}
BIND(GetMonitorInfoA,8);

/* NT 3.1 predates SPI_GETWORKAREA. Program Manager has no taskbar that
   reserves desktop space. Keep native results on systems that implement it. */
static BOOL legacy_work_area(UINT action,PVOID data) {
  DWORD version=GetVersion();RECT *rect=(RECT*)data;
  if(action!=SPI_GETWORKAREA||!rect||(version&0x80000000UL)||(version&255)!=3)return FALSE;
  SetRect(rect,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
  return rect->right>0&&rect->bottom>0;
}
static BOOL WINAPI legacy_SystemParametersInfoA(UINT action,UINT param,PVOID data,UINT flags) {
  __typeof__(&SystemParametersInfoA) fn=(__typeof__(&SystemParametersInfoA))GetProcAddress(GetModuleHandleA("user32.dll"),"SystemParametersInfoA");
  if(fn&&fn(action,param,data,flags))return TRUE;
  return legacy_work_area(action,data);
}
BIND(SystemParametersInfoA,16);
static BOOL WINAPI legacy_SystemParametersInfoW(UINT action,UINT param,PVOID data,UINT flags) {
  __typeof__(&SystemParametersInfoW) fn=(__typeof__(&SystemParametersInfoW))GetProcAddress(GetModuleHandleA("user32.dll"),"SystemParametersInfoW");
  if(fn&&fn(action,param,data,flags))return TRUE;
  return legacy_work_area(action,data);
}
BIND(SystemParametersInfoW,16);


static HWND WINAPI legacy_CreateToolbarEx(HWND parent,DWORD style,UINT id,int count,HINSTANCE instance,UINT_PTR bitmap,LPCTBBUTTON buttons,int nbuttons,int dxbutton,int dybutton,int dxbitmap,int dybitmap,UINT structsize) {
  __typeof__(&CreateToolbarEx) fn=(__typeof__(&CreateToolbarEx))GetProcAddress(common_module(),"CreateToolbarEx");
  HWND toolbar;TBADDBITMAP image;
  if(fn)return fn(parent,style,id,count,instance,bitmap,buttons,nbuttons,dxbutton,dybutton,dxbitmap,dybitmap,structsize);
  if(structsize!=sizeof(TBBUTTON)||nbuttons<0||count<0){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
  InitCommonControls();
  toolbar=CreateWindowExA(0,TOOLBARCLASSNAMEA,NULL,style,0,0,0,0,parent,(HMENU)(UINT_PTR)id,GetModuleHandleA(NULL),NULL);
  if(!toolbar)return NULL;
  SendMessageA(toolbar,TB_BUTTONSTRUCTSIZE,structsize,0);
  if(dxbitmap&&dybitmap)SendMessageA(toolbar,TB_SETBITMAPSIZE,0,MAKELPARAM(dxbitmap,dybitmap));
  if(dxbutton&&dybutton)SendMessageA(toolbar,TB_SETBUTTONSIZE,0,MAKELPARAM(dxbutton,dybutton));
  if(count){image.hInst=instance;image.nID=bitmap;if(SendMessageA(toolbar,TB_ADDBITMAP,count,(LPARAM)&image)<0){DestroyWindow(toolbar);return NULL;}}
  if(nbuttons&&!SendMessageA(toolbar,TB_ADDBUTTONSA,nbuttons,(LPARAM)buttons)){DestroyWindow(toolbar);return NULL;}
  SendMessageA(toolbar,TB_AUTOSIZE,0,0);return toolbar;
}
BIND(CreateToolbarEx,52);

/* Win95 exports MessageBoxW but does not implement it. Keep diagnostic and
   overwrite/error messages visible on that system. NT uses native Unicode. */
static int WINAPI legacy_MessageBoxW(HWND owner,LPCWSTR text,LPCWSTR caption,UINT flags)
{
  __typeof__(&MessageBoxW) fn=(__typeof__(&MessageBoxW))GetProcAddress(GetModuleHandleA("user32.dll"),"MessageBoxW");
  char *a=NULL,*b=NULL;int n,result=0;
  if(!(GetVersion()&0x80000000UL))return fn(owner,text,caption,flags);
  if(text){n=WideCharToMultiByte(CP_ACP,0,text,-1,NULL,0,NULL,NULL);if(!n)return 0;a=HeapAlloc(GetProcessHeap(),0,n);if(!a){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}WideCharToMultiByte(CP_ACP,0,text,-1,a,n,NULL,NULL);}
  if(caption){n=WideCharToMultiByte(CP_ACP,0,caption,-1,NULL,0,NULL,NULL);if(!n)goto done;b=HeapAlloc(GetProcessHeap(),0,n);if(!b){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;}WideCharToMultiByte(CP_ACP,0,caption,-1,b,n,NULL,NULL);}
  result=MessageBoxA(owner,a,b,flags);
 done:if(a)HeapFree(GetProcessHeap(),0,a);if(b)HeapFree(GetProcessHeap(),0,b);return result;
}
BIND(MessageBoxW,16);
