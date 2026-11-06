/* Modal property sheets for NT 3.1. The selector uses a native combo box;
   oversized resource pages scroll inside the available desktop area.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <prsht.h>
#define BIND(name,bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes) = legacy_##name
#define PAGE_SELECT 7100
#define PAGE_APPLY 7101
#define PAGE_HELP 7102
typedef struct Page {HWND window;WNDPROC original;int width,height,x,y;BOOL dirty,wide;} Page;
typedef struct Sheet {HWND window,selector,ok,cancel,apply,help;Page *pages;UINT count,current;BOOL done,wide;INT_PTR result;DWORD flags;HFONT font;} Sheet;
static char *ansi(LPCWSTR value){int n;char *p;if(!value)return NULL;n=WideCharToMultiByte(CP_ACP,0,value,-1,NULL,0,NULL,NULL);if(!n)return NULL;p=HeapAlloc(GetProcessHeap(),0,n);if(p)WideCharToMultiByte(CP_ACP,0,value,-1,p,n,NULL,NULL);return p;}
static void free_memory(void *p){if(p)HeapFree(GetProcessHeap(),0,p);}
static LRESULT notify_page(Sheet *s,UINT index,UINT code,LPARAM param){PSHNOTIFY info;Page *p;if(index>=s->count)return 0;p=s->pages+index;
 ZeroMemory(&info,sizeof(info));info.hdr.hwndFrom=s->window;info.hdr.code=code;info.lParam=param;SetWindowLongA(p->window,DWL_MSGRESULT,0);
 SendMessageA(p->window,WM_NOTIFY,0,(LPARAM)&info);return GetWindowLongA(p->window,DWL_MSGRESULT);}
static void update_apply(Sheet *s){UINT i;BOOL dirty=FALSE;for(i=0;i<s->count;i++)dirty|=s->pages[i].dirty;EnableWindow(s->apply,dirty);}
static void scroll_page(Page *page,int dx,int dy){HWND child;RECT r;
 for(child=GetWindow(page->window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){GetWindowRect(child,&r);MapWindowPoints(NULL,page->window,(POINT*)&r,2);SetWindowPos(child,NULL,r.left-dx,r.top-dy,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 page->x+=dx;page->y+=dy;SetScrollPos(page->window,SB_HORZ,page->x,TRUE);SetScrollPos(page->window,SB_VERT,page->y,TRUE);InvalidateRect(page->window,NULL,TRUE);}
static void page_ranges(Page *p){RECT r;int maxx,maxy;GetClientRect(p->window,&r);maxx=p->width-r.right;maxy=p->height-r.bottom;if(maxx<0)maxx=0;if(maxy<0)maxy=0;
 SetScrollRange(p->window,SB_HORZ,0,maxx,TRUE);SetScrollRange(p->window,SB_VERT,0,maxy,TRUE);
 if(p->x>maxx||p->y>maxy)scroll_page(p,p->x>maxx?maxx-p->x:0,p->y>maxy?maxy-p->y:0);}
static LRESULT CALLBACK page_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){Page *p=(Page*)GetPropA(w,"Z7PropertyPage");
 if(!p)return DefWindowProcA(w,msg,wp,lp);
 if((msg==WM_VSCROLL||msg==WM_HSCROLL)&&!lp){int low,high,pos=msg==WM_HSCROLL?p->x:p->y;RECT r;GetClientRect(w,&r);GetScrollRange(w,msg==WM_HSCROLL?SB_HORZ:SB_VERT,&low,&high);
  switch(LOWORD(wp)){case SB_LINEUP:pos-=16;break;case SB_LINEDOWN:pos+=16;break;case SB_PAGEUP:pos-=msg==WM_HSCROLL?r.right:r.bottom;break;case SB_PAGEDOWN:pos+=msg==WM_HSCROLL?r.right:r.bottom;break;case SB_THUMBPOSITION:case SB_THUMBTRACK:pos=HIWORD(wp);break;case SB_TOP:pos=low;break;case SB_BOTTOM:pos=high;break;}
  if(pos<low)pos=low;if(pos>high)pos=high;scroll_page(p,msg==WM_HSCROLL?pos-p->x:0,msg==WM_VSCROLL?pos-p->y:0);return 0;
 }
 if(msg==WM_SIZE)page_ranges(p);
 return p->wide?CallWindowProcW(p->original,w,msg,wp,lp):CallWindowProcA(p->original,w,msg,wp,lp);
}
static void layout(Sheet *s){RECT r;UINT i;int width,height,button=76,x;GetClientRect(s->window,&r);width=r.right-20;height=r.bottom-90;if(width<80)width=80;if(height<40)height=40;
 MoveWindow(s->selector,10,10,width,200,TRUE);
 for(i=0;i<s->count;i++){MoveWindow(s->pages[i].window,10,40,width,height,TRUE);page_ranges(s->pages+i);}
 x=r.right-10-button;MoveWindow(s->cancel,x,r.bottom-34,button,24,TRUE);x-=button+8;MoveWindow(s->ok,x,r.bottom-34,button,24,TRUE);
 if(!(s->flags&PSH_NOAPPLYNOW)){x-=button+8;MoveWindow(s->apply,x,r.bottom-34,button,24,TRUE);}
 MoveWindow(s->help,10,r.bottom-34,button,24,TRUE);
}
static BOOL select_page(Sheet *s,UINT index){UINT old=s->current;if(index>=s->count)return FALSE;if(old==index)return TRUE;
 if(old<s->count&&notify_page(s,old,PSN_KILLACTIVE,0)){SendMessageA(s->selector,CB_SETCURSEL,old,0);return FALSE;}
 if(notify_page(s,index,PSN_SETACTIVE,0)!=0){SendMessageA(s->selector,CB_SETCURSEL,old,0);return FALSE;}
 if(old<s->count)ShowWindow(s->pages[old].window,SW_HIDE);s->current=index;ShowWindow(s->pages[index].window,SW_SHOW);SendMessageA(s->selector,CB_SETCURSEL,index,0);return TRUE;
}
static BOOL apply_pages(Sheet *s,BOOL close){UINT i;LRESULT result;if(s->current<s->count&&notify_page(s,s->current,PSN_KILLACTIVE,0))return FALSE;
 for(i=0;i<s->count;i++){result=notify_page(s,i,PSN_APPLY,close);if(result!=PSNRET_NOERROR){if(result==PSNRET_INVALID)select_page(s,i);return FALSE;}}
 for(i=0;i<s->count;i++)s->pages[i].dirty=FALSE;update_apply(s);if(close){s->done=TRUE;if(!s->result)s->result=IDOK;}return TRUE;
}
static void cancel_sheet(Sheet *s){UINT i;if(s->current<s->count&&notify_page(s,s->current,PSN_QUERYCANCEL,0))return;for(i=0;i<s->count;i++)notify_page(s,i,PSN_RESET,0);s->done=TRUE;s->result=0;}
static LRESULT CALLBACK sheet_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){Sheet *s=(Sheet*)GetWindowLongA(w,0);UINT i;
 if(msg==WM_CREATE){s=(Sheet*)((CREATESTRUCTA*)lp)->lpCreateParams;SetWindowLongA(w,0,(LONG)s);s->window=w;return 0;}
 if(!s)return DefWindowProcA(w,msg,wp,lp);
 switch(msg){
 case WM_SIZE:if(s->selector)layout(s);return 0;
 case WM_CLOSE:cancel_sheet(s);return 0;
 case WM_COMMAND:
  if(LOWORD(wp)==PAGE_SELECT&&HIWORD(wp)==CBN_SELCHANGE){select_page(s,SendMessageA(s->selector,CB_GETCURSEL,0,0));return 0;}
  switch(LOWORD(wp)){case IDOK:apply_pages(s,TRUE);return 0;case IDCANCEL:cancel_sheet(s);return 0;case PAGE_APPLY:apply_pages(s,FALSE);return 0;case PAGE_HELP:if(s->current<s->count)notify_page(s,s->current,PSN_HELP,0);return 0;}break;
 case PSM_CHANGED:case PSM_UNCHANGED:for(i=0;i<s->count;i++)if(s->pages[i].window==(HWND)wp)s->pages[i].dirty=msg==PSM_CHANGED;update_apply(s);return 0;
 case PSM_APPLY:return apply_pages(s,FALSE);
 case PSM_SETCURSEL:if(lp){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}return select_page(s,wp);
 case PSM_GETCURRENTPAGEHWND:return s->current<s->count?(LRESULT)s->pages[s->current].window:0;
 case PSM_PRESSBUTTON:switch(wp){case PSBTN_OK:apply_pages(s,TRUE);break;case PSBTN_CANCEL:cancel_sheet(s);break;case PSBTN_APPLYNOW:apply_pages(s,FALSE);break;case PSBTN_HELP:if(s->current<s->count)notify_page(s,s->current,PSN_HELP,0);break;}return 0;
 case PSM_RESTARTWINDOWS:s->result=ID_PSRESTARTWINDOWS;return 0;
 case PSM_REBOOTSYSTEM:s->result=ID_PSREBOOTSYSTEM;return 0;
 }
 return DefWindowProcA(w,msg,wp,lp);
}
static void tab_focus(Sheet *s,BOOL backward){
 HWND current=GetFocus(),next=NULL,first=NULL,last=NULL,buttons[4];UINT i;int index=-1,step=backward?-1:1;
 if(s->current<s->count){HWND child;for(child=GetWindow(s->pages[s->current].window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))if((GetWindowLongA(child,GWL_STYLE)&WS_TABSTOP)&&IsWindowVisible(child)&&IsWindowEnabled(child)){if(!first)first=child;last=child;}}
 buttons[0]=s->ok;buttons[1]=s->cancel;buttons[2]=s->apply;buttons[3]=s->help;
 if(current==s->selector){if(!backward)next=first?first:s->ok;else{for(i=4;i>0;i--)if(IsWindowVisible(buttons[i-1])&&IsWindowEnabled(buttons[i-1])){next=buttons[i-1];break;}}}
 else if(s->current<s->count&&IsChild(s->pages[s->current].window,current)){
  next=GetNextDlgTabItem(s->pages[s->current].window,current,backward);
  if((backward&&next==last)||(!backward&&next==first))next=backward?s->selector:s->ok;
 }else{
  for(i=0;i<4;i++)if(buttons[i]==current)index=i;
  for(index+=step;index>=0&&index<4;index+=step)if(IsWindowVisible(buttons[index])&&IsWindowEnabled(buttons[index])){next=buttons[index];break;}
  if(!next)next=index<0?(last?last:s->selector):s->selector;
 }
 if(!next)next=s->selector;
 SetFocus(next);
 if(s->current<s->count&&IsChild(s->pages[s->current].window,next)){
  Page *p=s->pages+s->current;RECT r,client;int dx=0,dy=0,low,high;
  GetWindowRect(next,&r);MapWindowPoints(NULL,p->window,(POINT*)&r,2);GetClientRect(p->window,&client);
  if(r.left<0)dx=r.left;else if(r.right>client.right)dx=r.right-client.right;
  if(r.top<0)dy=r.top;else if(r.bottom>client.bottom)dy=r.bottom-client.bottom;
  GetScrollRange(p->window,SB_HORZ,&low,&high);if(p->x+dx>high)dx=high-p->x;if(p->x+dx<low)dx=low-p->x;
  GetScrollRange(p->window,SB_VERT,&low,&high);if(p->y+dy>high)dy=high-p->y;if(p->y+dy<low)dy=low-p->y;
  if(dx||dy)scroll_page(p,dx,dy);
 }
}
static INT_PTR property_sheet(const PROPSHEETHEADERA *header,BOOL wide){
 Sheet s;WNDCLASSA wc;HINSTANCE module=GetModuleHandleA(NULL);const BYTE *source;UINT i;char *caption=NULL;RECT work,rect;int width=480,height=360;BOOL owner_enabled=FALSE,has_help=FALSE;MSG msg;INT_PTR result=-1;DWORD error=0;
 if(!header||header->dwSize<PROPSHEETHEADERA_V1_SIZE||!header->nPages||header->nPages>128||!header->ppsp||!(header->dwFlags&PSH_PROPSHEETPAGE)){SetLastError(ERROR_INVALID_PARAMETER);return -1;}
 if(header->dwFlags&~(PSH_PROPSHEETPAGE|PSH_NOAPPLYNOW|PSH_HASHELP)){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return -1;}
 ZeroMemory(&s,sizeof(s));s.wide=wide;s.flags=header->dwFlags;s.count=header->nPages;s.current=s.count;s.font=GetStockObject(ANSI_VAR_FONT);if(!s.font)s.font=GetStockObject(SYSTEM_FONT);
 s.pages=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,s.count*sizeof(Page));if(!s.pages){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return -1;}
 ZeroMemory(&wc,sizeof(wc));if(!GetClassInfoA(module,"Z7PropertySheet",&wc)){wc.lpfnWndProc=sheet_proc;wc.cbWndExtra=sizeof(void*);wc.hInstance=module;wc.hCursor=LoadCursorA(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName="Z7PropertySheet";if(!RegisterClassA(&wc))goto cleanup;}
 if(wide)caption=ansi((LPCWSTR)header->pszCaption);
 s.window=CreateWindowExA(WS_EX_DLGMODALFRAME,"Z7PropertySheet",wide?caption:header->pszCaption,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,width,height,header->hwndParent,NULL,module,&s);free_memory(caption);if(!s.window)goto cleanup;
 s.selector=CreateWindowA("COMBOBOX","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,10,10,400,200,s.window,(HMENU)PAGE_SELECT,module,NULL);
 s.ok=CreateWindowA("BUTTON","OK",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,0,0,76,24,s.window,(HMENU)IDOK,module,NULL);
 s.cancel=CreateWindowA("BUTTON","Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,76,24,s.window,(HMENU)IDCANCEL,module,NULL);
 s.apply=CreateWindowA("BUTTON","Apply",WS_CHILD|WS_TABSTOP|((s.flags&PSH_NOAPPLYNOW)?0:WS_VISIBLE),0,0,76,24,s.window,(HMENU)PAGE_APPLY,module,NULL);
 s.help=CreateWindowA("BUTTON","Help",WS_CHILD|WS_TABSTOP,0,0,76,24,s.window,(HMENU)PAGE_HELP,module,NULL);
 if(!s.selector||!s.ok||!s.cancel||!s.apply||!s.help)goto cleanup;
 SendMessageA(s.selector,WM_SETFONT,(WPARAM)s.font,0);SendMessageA(s.ok,WM_SETFONT,(WPARAM)s.font,0);SendMessageA(s.cancel,WM_SETFONT,(WPARAM)s.font,0);SendMessageA(s.apply,WM_SETFONT,(WPARAM)s.font,0);SendMessageA(s.help,WM_SETFONT,(WPARAM)s.font,0);
 source=(const BYTE*)header->ppsp;
 for(i=0;i<s.count;i++){
  const PROPSHEETPAGEA *page=(const PROPSHEETPAGEA*)source;Page *p=s.pages+i;char title[256];const char *text=title;char *converted=NULL;LONG style;
  if(page->dwSize<PROPSHEETPAGEA_V1_SIZE||!page->pfnDlgProc){SetLastError(ERROR_INVALID_PARAMETER);goto cleanup;}
  if(page->dwFlags&~(PSP_HASHELP|PSP_USETITLE|PSP_DLGINDIRECT)){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);goto cleanup;}
  p->wide=wide;
  if(page->dwFlags&PSP_DLGINDIRECT)p->window=wide?CreateDialogIndirectParamW(page->hInstance,page->pResource,s.window,page->pfnDlgProc,(LPARAM)page):CreateDialogIndirectParamA(page->hInstance,page->pResource,s.window,page->pfnDlgProc,(LPARAM)page);
  else p->window=wide?CreateDialogParamW(page->hInstance,(LPCWSTR)page->pszTemplate,s.window,page->pfnDlgProc,(LPARAM)page):CreateDialogParamA(page->hInstance,page->pszTemplate,s.window,page->pfnDlgProc,(LPARAM)page);
  if(!p->window)goto cleanup;
  GetClientRect(p->window,&rect);p->width=rect.right;p->height=rect.bottom;if(p->width+40>width)width=p->width+40;if(p->height+130>height)height=p->height+130;
  style=GetWindowLongA(p->window,GWL_STYLE);SetWindowLongA(p->window,GWL_STYLE,(style&~(WS_CAPTION|WS_POPUP|WS_VISIBLE))|WS_CHILD|WS_HSCROLL|WS_VSCROLL);
  if(!SetPropA(p->window,"Z7PropertyPage",p))goto cleanup;
  p->original=(WNDPROC)(wide?SetWindowLongW(p->window,GWL_WNDPROC,(LONG)page_proc):SetWindowLongA(p->window,GWL_WNDPROC,(LONG)page_proc));
  title[0]=0;GetWindowTextA(p->window,title,sizeof(title));
  if(page->dwFlags&PSP_USETITLE){if(wide){converted=ansi((LPCWSTR)page->pszTitle);text=converted;}else text=page->pszTitle;}
  if(SendMessageA(s.selector,CB_ADDSTRING,0,(LPARAM)(text?text:""))<0){free_memory(converted);goto cleanup;}free_memory(converted);
  has_help|=(page->dwFlags&PSP_HASHELP)!=0;source+=page->dwSize;
 }
 if(has_help||(s.flags&PSH_HASHELP))ShowWindow(s.help,SW_SHOW);
 SetRect(&work,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));SystemParametersInfoA(SPI_GETWORKAREA,0,&work,0);
 if(width>work.right-work.left-20)width=work.right-work.left-20;if(height>work.bottom-work.top-20)height=work.bottom-work.top-20;
 MoveWindow(s.window,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,TRUE);layout(&s);update_apply(&s);
 if(!select_page(&s,header->nStartPage<s.count?header->nStartPage:0))goto cleanup;
 if(header->hwndParent&&IsWindowEnabled(header->hwndParent)){EnableWindow(header->hwndParent,FALSE);owner_enabled=TRUE;}
 ShowWindow(s.window,SW_SHOW);SetActiveWindow(s.window);SetFocus(s.selector);
 while(!s.done){BOOL got=GetMessageA(&msg,NULL,0,0);if(got<=0){if(!got)PostQuitMessage((int)msg.wParam);else error=GetLastError();break;}
  if(msg.message==WM_KEYDOWN){
   if(msg.wParam==VK_ESCAPE){cancel_sheet(&s);continue;}
   if(msg.wParam==VK_TAB&&(GetKeyState(VK_CONTROL)&0x8000)){UINT target=(s.current+((GetKeyState(VK_SHIFT)&0x8000)?s.count-1:1))%s.count;select_page(&s,target);continue;}
   if(msg.wParam==VK_TAB){tab_focus(&s,(GetKeyState(VK_SHIFT)&0x8000)!=0);continue;}
   if(msg.wParam==VK_RETURN){
    HWND focus=GetFocus();char class_name[16];GetClassNameA(focus,class_name,sizeof(class_name));
    if(!lstrcmpiA(class_name,"EDIT")&&(GetWindowLongA(focus,GWL_STYLE)&ES_MULTILINE)){}
    else{if(GetParent(focus)==s.window&&!lstrcmpiA(class_name,"BUTTON"))SendMessageA(s.window,WM_COMMAND,GetDlgCtrlID(focus),(LPARAM)focus);else apply_pages(&s,TRUE);continue;}
   }
  }
  if(s.current<s.count&&msg.hwnd&&IsChild(s.pages[s.current].window,msg.hwnd)&&(wide?IsDialogMessageW(s.pages[s.current].window,&msg):IsDialogMessageA(s.pages[s.current].window,&msg)))continue;
  TranslateMessage(&msg);DispatchMessageA(&msg);
 }
 if(s.done)result=s.result;
cleanup:
 if(!error)error=GetLastError();
 if(owner_enabled){EnableWindow(header->hwndParent,TRUE);SetActiveWindow(header->hwndParent);}
 if(s.window)DestroyWindow(s.window);free_memory(s.pages);if(result==-1)SetLastError(error?error:ERROR_INVALID_DATA);return result;
}
static INT_PTR WINAPI legacy_PropertySheetA(const PROPSHEETHEADERA *header){
 HMODULE module=GetModuleHandleA("comctl32.dll");__typeof__(&PropertySheetA) fn;PROPSHEETHEADERA copy;
 if(!module)module=LoadLibraryA("comctl32.dll");fn=(__typeof__(&PropertySheetA))GetProcAddress(module,"PropertySheetA");
 if(!fn)return property_sheet(header,FALSE);
 if(header&&header->dwSize>=PROPSHEETHEADERA_V1_SIZE&&(GetVersion()&0x80000000UL)){ZeroMemory(&copy,sizeof(copy));CopyMemory(&copy,header,PROPSHEETHEADERA_V1_SIZE);copy.dwSize=PROPSHEETHEADERA_V1_SIZE;return fn(&copy);}return fn(header);
}
BIND(PropertySheetA,4);
static INT_PTR WINAPI legacy_PropertySheetW(const PROPSHEETHEADERW *header){
 HMODULE module=GetModuleHandleA("comctl32.dll");__typeof__(&PropertySheetW) fn;
 if(!module)module=LoadLibraryA("comctl32.dll");fn=(__typeof__(&PropertySheetW))GetProcAddress(module,"PropertySheetW");return fn?fn(header):property_sheet((const PROPSHEETHEADERA*)header,TRUE);
}
BIND(PropertySheetW,4);
