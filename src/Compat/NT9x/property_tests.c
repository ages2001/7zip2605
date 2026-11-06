/* Native modal property-sheet lifecycle regression test. No CRT.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <prsht.h>
void *memset(void *p,int v,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=(BYTE)v;return p;}
void *memcpy(void *d,const void *s,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)d)[i]=((const BYTE*)s)[i];return d;}
static HANDLE report;static HWND sheet,pages[2];static UINT_PTR timer;static unsigned mode,phase,apply_count[2],reset_count,help_count;static BOOL block_leave,block_apply;
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);}
static void hex(DWORD value){char s[9];unsigned i;for(i=0;i<8;i++)s[i]="0123456789ABCDEF"[(value>>(28-4*i))&15];s[8]=0;line(s);}
static void check(BOOL ok,const char *s){if(!ok){line("FAIL");line(s);CloseHandle(report);ExitProcess(1);}}
static void CALLBACK tick(HWND w,UINT msg,UINT_PTR id,DWORD now){LRESULT result;(void)w;(void)msg;(void)id;(void)now;
 if(mode){KillTimer(NULL,timer);timer=0;SendMessageA(sheet,PSM_CHANGED,(WPARAM)pages[0],0);PostMessageA(sheet,PSM_PRESSBUTTON,PSBTN_CANCEL,0);return;}
 switch(phase++){
 case 0:
  check(pages[0]!=NULL,"initial page exists");SendMessageA(sheet,PSM_CHANGED,(WPARAM)pages[0],0);
  check(SendMessageA(sheet,PSM_APPLY,0,0)&&apply_count[0]==1,"apply notification");
  SetFocus(GetDlgItem(pages[0],1002));PostMessageA(GetFocus(),WM_KEYDOWN,VK_TAB,0);break;
 case 1:
  if(GetFocus()!=GetDlgItem(sheet,IDOK)){hex(GetDlgCtrlID(GetFocus()));hex(GetDlgCtrlID(GetNextDlgTabItem(pages[0],NULL,FALSE)));hex(GetDlgCtrlID(GetNextDlgTabItem(pages[0],GetDlgItem(pages[0],1002),FALSE)));}
  check(GetFocus()==GetDlgItem(sheet,IDOK),"Tab leaves page for OK button");
  block_leave=TRUE;result=SendMessageA(sheet,PSM_SETCURSEL,1,0);check(!result&&IsWindowVisible(pages[0]),"page validation blocks navigation");block_leave=FALSE;
  check(SendMessageA(sheet,PSM_SETCURSEL,1,0),"select second page");break;
 case 2:
  check(pages[1]&&IsWindowVisible(pages[1])&&!IsWindowVisible(pages[0]),"second page visibility");
  if((GetVersion()&0xffff)==0x0a03){RECT child,client;int low,high;
   GetScrollRange(pages[1],SB_VERT,&low,&high);check(high>0,"oversized page has vertical scrolling");SendMessageA(pages[1],WM_VSCROLL,SB_BOTTOM,0);
   GetWindowRect(GetDlgItem(pages[1],1004),&child);MapWindowPoints(NULL,pages[1],(POINT*)&child,2);GetClientRect(pages[1],&client);
   check(child.top>=0&&child.bottom<=client.bottom,"scroll reaches bottom control");
  }
  SendMessageA(sheet,PSM_PRESSBUTTON,PSBTN_HELP,0);check(help_count==1,"help notification");
  SendMessageA(sheet,PSM_CHANGED,(WPARAM)pages[1],0);block_apply=TRUE;
  check(!SendMessageA(sheet,PSM_APPLY,0,0)&&IsWindow(sheet),"invalid settings keep sheet open");block_apply=FALSE;
  KillTimer(NULL,timer);timer=0;PostMessageA(sheet,PSM_PRESSBUTTON,PSBTN_OK,0);break;
 default:check(FALSE,"unexpected timer phase");
 }
}
static INT_PTR CALLBACK page_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){unsigned index=(unsigned)GetWindowLongA(w,DWL_USER);(void)wp;
 if(msg==WM_INITDIALOG){const PROPSHEETPAGEA *p=(const PROPSHEETPAGEA*)lp;index=p->lParam;check(index<2,"page initialization parameter");SetWindowLongA(w,DWL_USER,index);pages[index]=w;return TRUE;}
 if(msg==WM_NOTIFY){const PSHNOTIFY *n=(const PSHNOTIFY*)lp;switch(n->hdr.code){
  case PSN_SETACTIVE:sheet=GetParent(w);if(!timer)timer=SetTimer(NULL,0,100,tick);SetWindowLongA(w,DWL_MSGRESULT,0);return TRUE;
  case PSN_KILLACTIVE:SetWindowLongA(w,DWL_MSGRESULT,block_leave);return TRUE;
  case PSN_APPLY:apply_count[index]++;SetWindowLongA(w,DWL_MSGRESULT,block_apply?PSNRET_INVALID_NOCHANGEPAGE:PSNRET_NOERROR);return TRUE;
  case PSN_RESET:reset_count++;return TRUE;
  case PSN_HELP:help_count++;return TRUE;
  case PSN_QUERYCANCEL:SetWindowLongA(w,DWL_MSGRESULT,FALSE);return TRUE;
 }}return FALSE;
}
void entry(void){PROPSHEETHEADERA header;PROPSHEETHEADERW wide_header;PROPSHEETPAGEA_V1 page[2];PROPSHEETPAGEW_V1 wide_page[2];unsigned i,variant;INT_PTR result;HWND owner;
 report=CreateFileA("C:\\PROPTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 owner=CreateWindowA("STATIC","Owner",WS_OVERLAPPEDWINDOW,0,0,100,100,NULL,NULL,GetModuleHandleA(NULL),NULL);check(owner!=NULL,"create owner");
 ZeroMemory(&header,sizeof(header));ZeroMemory(page,sizeof(page));header.dwSize=PROPSHEETHEADERA_V1_SIZE;header.dwFlags=PSH_PROPSHEETPAGE;header.hwndParent=owner;header.hInstance=GetModuleHandleA(NULL);header.pszCaption="Property regression test";header.nPages=2;header.ppsp=(LPCPROPSHEETPAGEA)page;
 for(i=0;i<2;i++){page[i].dwSize=sizeof(page[i]);page[i].dwFlags=PSP_HASHELP|PSP_USETITLE;page[i].hInstance=header.hInstance;page[i].pszTemplate=MAKEINTRESOURCEA(101+i);page[i].pszTitle=i?"Advanced":"General";page[i].pfnDlgProc=page_proc;page[i].lParam=i;}
 ZeroMemory(&wide_header,sizeof(wide_header));ZeroMemory(wide_page,sizeof(wide_page));wide_header.dwSize=PROPSHEETHEADERW_V1_SIZE;wide_header.dwFlags=PSH_PROPSHEETPAGE;wide_header.hwndParent=owner;wide_header.hInstance=header.hInstance;wide_header.pszCaption=L"Property regression test";wide_header.nPages=2;wide_header.ppsp=(LPCPROPSHEETPAGEW)wide_page;
 for(i=0;i<2;i++){wide_page[i].dwSize=sizeof(wide_page[i]);wide_page[i].dwFlags=PSP_HASHELP|PSP_USETITLE;wide_page[i].hInstance=header.hInstance;wide_page[i].pszTemplate=MAKEINTRESOURCEW(101+i);wide_page[i].pszTitle=i?L"Advanced":L"General";wide_page[i].pfnDlgProc=page_proc;wide_page[i].lParam=i;}
 for(variant=0;variant<2;variant++){
  if(variant&&(GetVersion()&0x80000000UL)){line("SKIP Unicode property-sheet API on Windows 95");break;}
  line(variant?"TEST Unicode property sheet":"TEST ANSI property sheet");mode=phase=help_count=reset_count=apply_count[0]=apply_count[1]=0;pages[0]=pages[1]=NULL;timer=0;
  result=variant?PropertySheetW(&wide_header):PropertySheetA(&header);check(result==IDOK,"OK result");check(IsWindowEnabled(owner),"owner re-enabled after OK");check(apply_count[0]>=2&&apply_count[1]>=1,"all visited pages applied");line("PASS apply, validation veto, page switching, help, OK, owner restoration");
 mode=1;phase=0;pages[0]=pages[1]=NULL;result=variant?PropertySheetW(&wide_header):PropertySheetA(&header);check(result==0&&reset_count>=1,"cancel resets visited pages");check(IsWindowEnabled(owner),"owner re-enabled after cancel");line("PASS cancel, reset, owner restoration");}
 DestroyWindow(owner);line("PASS ALL");CloseHandle(report);ExitProcess(0);
}
