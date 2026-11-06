/* Native tests for image lists, status bars and toolbars. No CRT.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
void *memset(void *p,int v,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=(BYTE)v;return p;}
void *memcpy(void *d,const void *s,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)d)[i]=((const BYTE*)s)[i];return d;}
extern BOOL LegacyImageListDraw(HIMAGELIST,int,HDC,int,int);
static HANDLE report;
static WNDPROC original_parent;
static unsigned commands,spinChanges;static BOOL vetoSpin;
static LRESULT CALLBACK parent_proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
 if(m==WM_NOTIFY&&((NMHDR*)lp)->code==UDN_DELTAPOS){spinChanges++;return vetoSpin;}
 if(m==WM_COMMAND&&LOWORD(wp)==502){commands++;return 0;}
 return CallWindowProcA(original_parent,w,m,wp,lp);
}
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);}
static void hex(DWORD value){char s[9];unsigned i;for(i=0;i<8;i++)s[i]="0123456789ABCDEF"[(value>>(28-4*i))&15];s[8]=0;line(s);}
static void check(BOOL b,const char *s){if(!b){line("FAIL");line(s);CloseHandle(report);ExitProcess(1);}}
void entry(void){
 HWND parent,status,toolbar;TBBUTTON button;char text[80];int parts[2]={100,-1};
 HIMAGELIST list;HDC screen,dc;HBITMAP bitmap;HGDIOBJ old;HBRUSH brush;RECT r;HICON icon;UINT pixel;
 report=CreateFileA("C:\\CTRLTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 line("BEGIN");InitCommonControls();line("CONTROLS INITIALIZED");parent=CreateWindowA("STATIC","Control test",WS_OVERLAPPEDWINDOW,0,0,400,300,NULL,NULL,NULL,NULL);check(parent!=NULL,"parent window");
 {RECT work;check(SystemParametersInfoA(SPI_GETWORKAREA,0,&work,0)&&work.right>work.left&&work.bottom>work.top,"ANSI desktop work area");
  if(!(GetVersion()&0x80000000UL))check(SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0)&&work.right>work.left&&work.bottom>work.top,"Unicode desktop work area");
  line("PASS valid desktop work area for automatic toolbar labels");}
 original_parent=(WNDPROC)SetWindowLongA(parent,GWL_WNDPROC,(LONG)parent_proc);
 line("PARENT CREATED");status=CreateStatusWindowW(WS_CHILD|WS_VISIBLE,L"Ready",parent,501);check(status!=NULL,"create status bar");
 line("STATUS CREATED");check(SendMessageA(status,SB_SETPARTS,2,(LPARAM)parts),"status parts");
 check(SendMessageA(status,SB_SETTEXTW,1,(LPARAM)L"Checked"),"status Unicode text");
 SendMessageA(status,SB_GETTEXTA,1,(LPARAM)text);check(!lstrcmpA(text,"Checked"),"status text round trip");
 ZeroMemory(&button,sizeof(button));button.iBitmap=0;button.idCommand=502;button.fsState=TBSTATE_ENABLED;button.fsStyle=TBSTYLE_BUTTON;
 screen=GetDC(NULL);dc=CreateCompatibleDC(screen);bitmap=CreateCompatibleBitmap(screen,32,16);check(screen&&dc&&bitmap,"create bitmap strip");old=SelectObject(dc,bitmap);
 SetRect(&r,0,0,32,16);brush=CreateSolidBrush(RGB(255,0,255));FillRect(dc,&r,brush);DeleteObject(brush);
 SetRect(&r,2,2,14,14);brush=CreateSolidBrush(RGB(255,0,0));FillRect(dc,&r,brush);DeleteObject(brush);
 SetRect(&r,18,2,30,14);brush=CreateSolidBrush(RGB(0,255,0));FillRect(dc,&r,brush);DeleteObject(brush);SelectObject(dc,old);
 {HWND progress;PBRANGE range;progress=CreateWindowA(PROGRESS_CLASSA,"",WS_CHILD|WS_VISIBLE|PBS_SMOOTH,0,0,200,20,parent,(HMENU)600,GetModuleHandleA(NULL),NULL);check(progress!=NULL,"create progress control used by extraction dialogs");SendMessageA(progress,PBM_SETRANGE32,0,100000);SendMessageA(progress,PBM_GETRANGE,0,(LPARAM)&range);check(range.iLow==0&&range.iHigh==100000,"32-bit progress range");SendMessageA(progress,PBM_SETPOS,70000,0);check(SendMessageA(progress,PBM_GETPOS,0,0)==70000,"32-bit progress position");SendMessageA(progress,PBM_DELTAPOS,50000,0);check(SendMessageA(progress,PBM_GETPOS,0,0)==100000,"progress clamps at upper range");SendMessageA(progress,PBM_SETSTEP,1000,0);SendMessageA(progress,PBM_STEPIT,0,0);check(SendMessageA(progress,PBM_GETPOS,0,0)==1000,"progress step wraps");DestroyWindow(progress);line("PASS progress control and 32-bit range");}
 {HWND edit,spin;RECT before,after;BOOL invalid;edit=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER,10,10,100,24,parent,(HMENU)610,GetModuleHandleA(NULL),NULL);GetWindowRect(edit,&before);spin=CreateWindowA(UPDOWN_CLASSA,"",WS_CHILD|WS_VISIBLE|UDS_AUTOBUDDY|UDS_ALIGNRIGHT|UDS_SETBUDDYINT|UDS_ARROWKEYS|UDS_NOTHOUSANDS,110,10,16,24,parent,(HMENU)611,GetModuleHandleA(NULL),NULL);check(spin!=NULL,"create memory-limit spin control");check((HWND)SendMessageA(spin,UDM_GETBUDDY,0,0)==edit,"spin finds preceding edit buddy");GetWindowRect(edit,&after);check(after.right<before.right,"spin aligns beside edit");SendMessageA(spin,UDM_SETRANGE,0,MAKELPARAM(100,1));SendMessageA(spin,UDM_SETPOS,0,4);GetWindowTextA(edit,text,sizeof(text));check(!lstrcmpA(text,"4"),"spin writes buddy value");SendMessageA(edit,WM_KEYDOWN,VK_UP,0);check(LOWORD(SendMessageA(spin,UDM_GETPOS,0,0))==5&&spinChanges==1,"buddy arrow updates value and notifies parent");vetoSpin=TRUE;SendMessageA(edit,WM_KEYDOWN,VK_UP,0);check(LOWORD(SendMessageA(spin,UDM_GETPOS,0,0))==5,"parent can veto spin change");vetoSpin=FALSE;SetWindowTextA(edit,"invalid");check(HIWORD(SendMessageA(spin,UDM_GETPOS,0,0))!=0,"spin reports invalid buddy input");SendMessageA(spin,UDM_SETRANGE32,0,100000);SendMessageA(spin,UDM_SETPOS32,0,70000);check(SendMessageA(spin,UDM_GETPOS32,0,(LPARAM)&invalid)==70000&&!invalid,"32-bit spin range and value");DestroyWindow(spin);SetWindowTextA(edit,"17");check(GetWindowTextA(edit,text,sizeof(text))&&!lstrcmpA(text,"17"),"destroying spin restores edit procedure");DestroyWindow(edit);line("PASS spin buddy, alignment, keyboard, veto and lifetime");}
 line("STATUS READY");
 toolbar=CreateToolbarEx(parent,WS_CHILD|WS_VISIBLE,503,2,NULL,(UINT_PTR)bitmap,&button,1,24,24,16,16,sizeof(button));check(toolbar!=NULL,"create toolbar");line("TOOLBAR CREATED");
 check(SendMessageA(toolbar,TB_BUTTONCOUNT,0,0)==1,"toolbar button count");
 {RECT resized;MoveWindow(toolbar,0,0,350,0,TRUE);GetWindowRect(toolbar,&resized);check(resized.bottom>resized.top,"toolbar preserves automatic height after zero-height resize");}
 line("BUTTON COUNT CHECKED");
 ZeroMemory(&button,sizeof(button));check(SendMessageA(toolbar,TB_GETBUTTON,0,(LPARAM)&button)&&button.idCommand==502,"toolbar command identity");
 check(SendMessageA(toolbar,TB_ENABLEBUTTON,502,FALSE),"disable toolbar button");check(!SendMessageA(toolbar,TB_ISBUTTONENABLED,502,0),"disabled toolbar state");
 check(SendMessageA(toolbar,TB_ENABLEBUTTON,502,TRUE)&&SendMessageA(toolbar,TB_ISBUTTONENABLED,502,0),"enable toolbar button");
 {RECT rect;LPARAM point;check(SendMessageA(toolbar,TB_GETITEMRECT,0,(LPARAM)&rect),"button rectangle");point=MAKELPARAM((rect.left+rect.right)/2,(rect.top+rect.bottom)/2);
  SendMessageA(toolbar,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageA(toolbar,WM_LBUTTONUP,0,point);
  check(commands==1,"mouse click delivers command");
  SendMessageA(toolbar,TB_ENABLEBUTTON,502,FALSE);SendMessageA(toolbar,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageA(toolbar,WM_LBUTTONUP,0,point);
  check(commands==1,"disabled button does not deliver command");SendMessageA(toolbar,TB_ENABLEBUTTON,502,TRUE);
 }
 line("PASS status bar and toolbar creation, text, buttons, state, mouse commands");
 {UINT strips[]={IDB_VIEW_SMALL_COLOR,IDB_VIEW_LARGE_COLOR,IDB_STD_SMALL_COLOR,IDB_HIST_SMALL_COLOR};int counts[]={12,12,15,5};unsigned k;
  for(k=0;k<4;k++){HWND stock;ZeroMemory(&button,sizeof(button));button.iBitmap=k<2?VIEW_PARENTFOLDER:0;button.idCommand=504;button.fsState=TBSTATE_ENABLED;button.iString=-1;
   stock=CreateToolbarEx(parent,WS_CHILD|WS_VISIBLE,505,counts[k],HINST_COMMCTRL,strips[k],&button,1,0,0,0,0,sizeof(button));check(stock!=NULL,"create standard bitmap toolbar");
   check(SendMessageA(stock,TB_BUTTONCOUNT,0,0)==1,"standard bitmap button count");
   if(!GetProcAddress(GetModuleHandleA("comctl32.dll"),"CreateToolbarEx")){
    HIMAGELIST images=(HIMAGELIST)SendMessageA(stock,TB_GETIMAGELIST,0,0);check(images&&ImageList_GetImageCount(images)==counts[k],"complete standard bitmap strip");
   }
   DestroyWindow(stock);
  }
  line("PASS stock toolbar strips, sizes and parent-folder button");
 }

 list=ImageList_Create(16,16,ILC_COLOR|ILC_MASK,1,1);check(list!=NULL,"create image list");
 check(ImageList_AddMasked(list,bitmap,RGB(255,0,255))==0&&ImageList_GetImageCount(list)==2,"add two masked images");
 old=SelectObject(dc,bitmap);SetRect(&r,0,0,32,16);brush=CreateSolidBrush(RGB(0,0,255));FillRect(dc,&r,brush);DeleteObject(brush);
 check(LegacyImageListDraw(list,0,dc,0,0),"draw first icon");
 pixel=GetPixel(dc,0,0);hex(pixel);hex(GetPixel(dc,5,5));check(pixel==RGB(0,0,255),"transparent pixels preserve destination");check(GetPixel(dc,5,5)==RGB(255,0,0),"opaque red image pixels");
 check(LegacyImageListDraw(list,1,dc,16,0)&&GetPixel(dc,21,5)==RGB(0,255,0),"second icon pixels");SelectObject(dc,old);
 icon=LoadIconA(NULL,IDI_APPLICATION);check(icon!=NULL,"load standard icon");
 check(ImageList_ReplaceIcon(list,-1,icon)==2&&ImageList_GetImageCount(list)==3,"append resized icon");
 check(ImageList_ReplaceIcon(list,0,icon)==0&&ImageList_GetImageCount(list)==3,"replace icon preserves count");
 check(ImageList_Destroy(list),"destroy image list");DestroyWindow(parent);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);
 line("PASS image list ownership, mask transparency, bitmap splitting, icon replacement");line("PASS ALL");CloseHandle(report);ExitProcess(0);
}
