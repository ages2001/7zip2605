/* Native regression tests for menu and child-window fallbacks. No CRT.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0500
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <stddef.h>
void *memset(void *p,int value,unsigned size){unsigned i;for(i=0;i<size;i++)((unsigned char*)p)[i]=(unsigned char)value;return p;}
static HANDLE report;
static void log_line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);}
static void check(BOOL ok,const char *s){if(!ok){log_line("FAIL");log_line(s);CloseHandle(report);ExitProcess(1);}}
static void init(MENUITEMINFOA *i,UINT mask){ZeroMemory(i,sizeof(*i));i->cbSize=offsetof(MENUITEMINFOA,hbmpItem);i->fMask=mask;}
void entry(void){
 HMENU menu,sub;MENUITEMINFOA item;MENUITEMINFOW wide;char text[128];WCHAR wtext[128];UINT n;
 HWND parent,front,back;POINT pt={12,12};
 report=CreateFileA("C:\\MENUTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 menu=LoadMenuA(GetModuleHandleA(NULL),MAKEINTRESOURCEA(401));check(menu&&GetMenuItemCount(menu)==2,"load MENUEX resource");
 init(&item,MIIM_ID|MIIM_SUBMENU);check(GetMenuItemInfoA(menu,0,TRUE,&item)&&item.wID==510&&item.hSubMenu,"MENUEX popup identifier");
 init(&item,MIIM_ID|MIIM_TYPE);item.dwTypeData=text;item.cch=sizeof(text);check(GetMenuItemInfoA(menu,513,FALSE,&item)&&!lstrcmpA(text,"Leaf"),"MENUEX nested item text");check(DestroyMenu(menu),"destroy MENUEX tree");
 menu=CreatePopupMenu();sub=CreatePopupMenu();check(menu&&sub,"create menus");
 init(&item,MIIM_TYPE|MIIM_ID|MIIM_DATA);item.wID=100;item.dwTypeData="First";item.dwItemData=0x12345678;
 check(InsertMenuItemA(menu,0,TRUE,&item),"insert ANSI item");
 ZeroMemory(&wide,sizeof(wide));wide.cbSize=offsetof(MENUITEMINFOW,hbmpItem);wide.fMask=MIIM_TYPE|MIIM_ID;wide.wID=101;wide.dwTypeData=L"Second";
 /* Win95 has no working Unicode menu API. The application uses ANSI there. */
 if(!(GetVersion()&0x80000000UL))check(InsertMenuItemW(menu,1,TRUE,&wide),"insert Unicode item");
 else {item.wID=101;item.dwTypeData="Second";check(InsertMenuItemA(menu,1,TRUE,&item),"insert second ANSI item");}
 init(&item,MIIM_TYPE|MIIM_DATA|MIIM_ID);item.dwTypeData=text;item.cch=sizeof(text);
 check(GetMenuItemInfoA(menu,0,TRUE,&item),"read ANSI item");
 check(lstrcmpA(text,"First")==0&&item.dwItemData==0x12345678&&item.wID==100&&item.cch==5,"ANSI text and data");
 init(&item,MIIM_STATE);item.fState=MFS_DISABLED|MFS_CHECKED;
 check(SetMenuItemInfoA(menu,100,FALSE,&item),"set state by command");
 init(&item,MIIM_TYPE|MIIM_STATE|MIIM_DATA);item.dwTypeData=text;item.cch=sizeof(text);
 check(GetMenuItemInfoA(menu,0,TRUE,&item),"read changed item");
 check(!lstrcmpA(text,"First")&&(item.fState&MFS_CHECKED)&&(item.fState&MFS_DISABLED)&&item.dwItemData==0x12345678,"state update preserves text and data");
 init(&item,MIIM_TYPE);item.dwTypeData="Renamed";check(SetMenuItemInfoA(menu,0,TRUE,&item),"rename item");
 init(&item,MIIM_TYPE);check(GetMenuItemInfoA(menu,0,TRUE,&item)&&item.cch==7,"query string length");
 item.dwTypeData=text;item.cch=4;check(GetMenuItemInfoA(menu,0,TRUE,&item)&&item.cch==3&&!lstrcmpA(text,"Ren"),"bounded text retrieval");
 if(!(GetVersion()&0x80000000UL)) {
  wide.fMask=MIIM_TYPE;wide.dwTypeData=wtext;wide.cch=128;
  check(GetMenuItemInfoW(menu,101,FALSE,&wide)&&!lstrcmpW(wtext,L"Second"),"read Unicode item");
  wide.dwTypeData=L"Unicode \x03a9";check(SetMenuItemInfoW(menu,101,FALSE,&wide),"set non-ASCII Unicode");
  wide.dwTypeData=wtext;wide.cch=128;check(GetMenuItemInfoW(menu,101,FALSE,&wide)&&!lstrcmpW(wtext,L"Unicode \x03a9"),"preserve non-ASCII Unicode");
 }
 init(&item,MIIM_TYPE|MIIM_ID);item.wID=200;item.dwTypeData="Child";check(InsertMenuItemA(sub,0,TRUE,&item),"insert child");
 item.fMask|=MIIM_SUBMENU|MIIM_DATA;item.wID=300;item.hSubMenu=sub;item.dwTypeData="Popup";item.dwItemData=42;
 check(InsertMenuItemA(menu,0,TRUE,&item),"insert popup before tracked items");
 init(&item,MIIM_ID|MIIM_SUBMENU|MIIM_DATA);check(GetMenuItemInfoA(menu,0,TRUE,&item)&&item.wID==300&&item.hSubMenu==sub&&item.dwItemData==42,"popup fields");
 init(&item,MIIM_ID);check(GetMenuItemInfoA(menu,200,FALSE,&item)&&item.wID==200,"recursive command lookup");
 check(CheckMenuRadioItem(menu,100,101,101,MF_BYCOMMAND),"radio selection");
 init(&item,MIIM_STATE|MIIM_TYPE);item.dwTypeData=text;item.cch=sizeof(text);
 check(GetMenuItemInfoA(menu,101,FALSE,&item)&&(item.fState&MFS_CHECKED)&&(item.fType&MFT_RADIOCHECK),"selected radio state and type");
 check(GetMenuItemInfoA(menu,100,FALSE,&item)&&!(item.fState&MFS_CHECKED),"unselected radio state");
 check(RemoveMenu(menu,0,MF_BYPOSITION),"detach popup");
 init(&item,MIIM_ID|MIIM_DATA);check(GetMenuItemInfoA(menu,0,TRUE,&item)&&item.wID==100&&item.dwItemData==0x12345678,"metadata follows removal");
 check(IsMenu(sub)&&DestroyMenu(sub)&&DestroyMenu(menu),"menu ownership and cleanup");
 log_line("PASS ANSI/Unicode menu insert, query, update, submenu, radio, removal");
 parent=CreateWindowA("STATIC","Test",WS_POPUP,0,0,100,100,NULL,NULL,NULL,NULL);
 check(parent!=NULL,"create parent");
 back=CreateWindowA("STATIC","Back",WS_CHILD|WS_VISIBLE,0,0,50,50,parent,NULL,NULL,NULL);
 front=CreateWindowExA(WS_EX_TRANSPARENT,"STATIC","Front",WS_CHILD|WS_VISIBLE,0,0,50,50,parent,NULL,NULL,NULL);
 check(back&&front,"create children");SetWindowPos(front,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
 check(ChildWindowFromPointEx(parent,pt,CWP_ALL)==front,"front child hit");
 check(ChildWindowFromPointEx(parent,pt,CWP_SKIPTRANSPARENT)==back,"transparent child skipped");
 EnableWindow(front,FALSE);check(ChildWindowFromPointEx(parent,pt,CWP_SKIPDISABLED)==back,"disabled child skipped");
 ShowWindow(front,SW_HIDE);check(ChildWindowFromPointEx(parent,pt,CWP_SKIPINVISIBLE)==back,"hidden child skipped");
 pt.x=75;check(ChildWindowFromPointEx(parent,pt,CWP_ALL)==parent,"empty client returns parent");
 pt.x=101;check(ChildWindowFromPointEx(parent,pt,CWP_ALL)==NULL,"outside client returns null");
 check(DestroyWindow(parent),"destroy windows");log_line("PASS child hit testing");
 for(n=0;n<64;n++){menu=CreatePopupMenu();init(&item,MIIM_TYPE|MIIM_ID|MIIM_DATA);item.wID=n;item.dwTypeData="Reuse";item.dwItemData=n;check(InsertMenuItemA(menu,0,TRUE,&item)&&DestroyMenu(menu),"repeated handle lifecycle");}
 log_line("PASS repeated menu lifecycle");log_line("PASS ALL");CloseHandle(report);ExitProcess(0);
}
