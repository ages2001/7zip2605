/* List-view integration regression fixture; no CRT.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0500
#include <windows.h>
#include <commctrl.h>
void *memset(void *p,int c,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=(BYTE)c;return p;}
void *memcpy(void *d,const void *s,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)d)[i]=((const BYTE*)s)[i];return d;}
extern BOOL LegacyIsListView(HWND);
static HANDLE report;static BOOL wide,veto;static unsigned callbacks,changes,clicks,doubleClicks,columnClicks,begins,ends,draws,dragBegins,dragSelection;
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);}
static void pump(DWORD duration){DWORD start=GetTickCount();MSG message;do{while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}Sleep(10);}while(GetTickCount()-start<duration);}
static void clickAt(HWND window,POINT point){ClientToScreen(window,&point);SetCursorPos(point.x,point.y);mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);pump(50);}
static void check(BOOL ok,const char *s){if(!ok){line("FAIL");line(s);CloseHandle(report);ExitProcess(1);}line(s);}
static LRESULT CALLBACK parentProc(HWND w,UINT m,WPARAM wp,LPARAM lp)
{
  if(m==WM_NOTIFY){NMHDR *h=(NMHDR*)lp;
    switch(h->code){
    case LVN_GETDISPINFOW:{NMLVDISPINFOW *n=(NMLVDISPINFOW*)h;callbacks++;if(n->item.mask&LVIF_TEXT)lstrcpynW(n->item.pszText,n->item.iSubItem?L"Property":L"Callback",n->item.cchTextMax);if(n->item.mask&LVIF_IMAGE)n->item.iImage=0;return 0;}
    case LVN_GETDISPINFOA:{NMLVDISPINFOA *n=(NMLVDISPINFOA*)h;callbacks++;if(n->item.mask&LVIF_TEXT)lstrcpynA(n->item.pszText,n->item.iSubItem?"Property":"Callback",n->item.cchTextMax);if(n->item.mask&LVIF_IMAGE)n->item.iImage=0;return 0;}
    case LVN_ITEMCHANGING:return veto&&((NMLISTVIEW*)h)->lParam==30;
    case LVN_ITEMCHANGED:changes++;return 0;
    case NM_CLICK:clicks++;return 0;case NM_DBLCLK:doubleClicks++;return 0;case LVN_COLUMNCLICK:columnClicks++;return 0;
    case LVN_BEGINDRAG:dragBegins++;dragSelection=(unsigned)SendMessageA(h->hwndFrom,LVM_GETSELECTEDCOUNT,0,0);return 0;
    case LVN_BEGINLABELEDITA:case LVN_BEGINLABELEDITW:begins++;return 0;
    case LVN_ENDLABELEDITA:case LVN_ENDLABELEDITW:ends++;return TRUE;
    case NM_CUSTOMDRAW:draws++;return CDRF_DODEFAULT;
    }
  }
  return DefWindowProcA(w,m,wp,lp);
}
static LRESULT send(HWND w,UINT a,UINT b,WPARAM wp,LPARAM lp){return SendMessageA(w,wide?b:a,wp,lp);}
static int insert(HWND w,int at,const WCHAR *text,LPARAM param){LVITEMW item;char a[80];ZeroMemory(&item,sizeof(item));item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=at;item.lParam=param;
  if(wide||text==LPSTR_TEXTCALLBACKW)item.pszText=(WCHAR*)text;else{WideCharToMultiByte(CP_ACP,0,text,-1,a,sizeof(a),NULL,NULL);item.pszText=(WCHAR*)a;}
  return (int)send(w,LVM_INSERTITEMA,LVM_INSERTITEMW,0,(LPARAM)&item);
}
static BOOL textIs(HWND w,int index,const WCHAR *expected){LVITEMW item;WCHAR text[80];char a[160];ZeroMemory(&item,sizeof(item));item.iSubItem=0;item.pszText=wide?text:(WCHAR*)a;item.cchTextMax=80;
  send(w,LVM_GETITEMTEXTA,LVM_GETITEMTEXTW,index,(LPARAM)&item);if(!wide)MultiByteToWideChar(CP_ACP,0,a,-1,text,80);return !lstrcmpW(text,expected);
}
static void state(HWND w,int index,UINT value,UINT mask){LVITEMW item;ZeroMemory(&item,sizeof(item));item.state=value;item.stateMask=mask;SendMessageA(w,LVM_SETITEMSTATE,index,(LPARAM)&item);}
static int CALLBACK compare(LPARAM a,LPARAM b,LPARAM context){(void)context;return a<b?-1:a>b?1:0;}
void entry(void)
{
  WNDCLASSA wc;HWND parent,list,edit;LVCOLUMNW column;LVITEMW item;RECT rect,client;LVHITTESTINFO hit;int order[2]={1,0},readOrder[2]={0,0},i;
  report=CreateFileA("C:\\LISTTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
  wide=(GetVersion()&0x80000000UL)==0;line(wide?"TEST Unicode list view":"TEST ANSI list view");
  InitCommonControls();ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=parentProc;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="Z7ListTestParent";check(RegisterClassA(&wc)!=0,"PASS parent class");
  parent=CreateWindowA(wc.lpszClassName,"List view tests",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,440,320,NULL,NULL,wc.hInstance,NULL);check(parent!=NULL,"PASS parent window");
  if(wide)list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_EDITLABELS|LVS_SHOWSELALWAYS|LVS_SHAREIMAGELISTS,0,0,400,240,parent,(HMENU)100,wc.hInstance,NULL);
  else list=CreateWindowExA(0,WC_LISTVIEWA,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_EDITLABELS|LVS_SHOWSELALWAYS|LVS_SHAREIMAGELISTS,0,0,400,240,parent,(HMENU)100,wc.hInstance,NULL);
  check(list!=NULL,"PASS create list view");SendMessageA(list,CCM_SETUNICODEFORMAT,wide,0);
  ZeroMemory(&column,sizeof(column));column.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;column.cx=180;column.pszText=wide?L"Name":(WCHAR*)"Name";
  check(send(list,LVM_INSERTCOLUMNA,LVM_INSERTCOLUMNW,0,(LPARAM)&column)==0,"PASS first column");
  column.cx=120;column.iSubItem=1;column.pszText=wide?L"Value":(WCHAR*)"Value";
  check(send(list,LVM_INSERTCOLUMNA,LVM_INSERTCOLUMNW,1,(LPARAM)&column)==1,"PASS second column");
  check(insert(list,0,L"Zeta",30)==0&&insert(list,1,LPSTR_TEXTCALLBACKW,10)==1&&insert(list,2,wide?L"Alpha \x03a9":L"Alpha",20)==2,"PASS insert stored and callback items");
  check(SendMessageA(list,LVM_GETITEMCOUNT,0,0)==3,"PASS item count");
  check(textIs(list,0,L"Zeta")&&textIs(list,1,L"Callback")&&textIs(list,2,wide?L"Alpha \x03a9":L"Alpha"),"PASS text round trips and callback");
  ZeroMemory(&item,sizeof(item));item.mask=LVIF_PARAM;item.iItem=1;check(send(list,LVM_GETITEMA,LVM_GETITEMW,0,(LPARAM)&item)&&item.lParam==10,"PASS item data");
  state(list,1,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);state(list,2,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
  check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==2&&SendMessageA(list,LVM_GETNEXTITEM,-1,LVNI_FOCUSED)==2,"PASS multiple selection and unique focus");
  check(SendMessageA(list,LVM_GETNEXTITEM,1,LVNI_ABOVE)==0&&SendMessageA(list,LVM_GETNEXTITEM,1,LVNI_BELOW)==2,"PASS directional report navigation");
  check(SendMessageA(list,LVM_GETNEXTITEM,0,LVNI_ABOVE)==-1&&SendMessageA(list,LVM_GETNEXTITEM,2,LVNI_BELOW)==-1,"PASS directional boundary handling");
  check(SendMessageA(list,LVM_GETNEXTITEM,0,LVNI_BELOW|LVNI_SELECTED)==1,"PASS directional selection filter");
  veto=TRUE;state(list,0,LVIS_SELECTED,LVIS_SELECTED);veto=FALSE;check(!(SendMessageA(list,LVM_GETITEMSTATE,0,LVIS_SELECTED)&LVIS_SELECTED),"PASS parent veto of state change");
  SendMessageA(list,LVM_SETSELECTIONMARK,0,0);
  check(SendMessageA(list,LVM_SORTITEMS,0,(LPARAM)compare),"PASS sort operation");
  item.iItem=0;check(send(list,LVM_GETITEMA,LVM_GETITEMW,0,(LPARAM)&item)&&item.lParam==10,"PASS sort by application item data");
  if(LegacyIsListView(list))check(SendMessageA(list,LVM_GETSELECTIONMARK,0,0)==2,"PASS sort preserves selection mark identity");
  SendMessageA(list,LVM_SETSELECTIONMARK,0,0);
  if(LegacyIsListView(list))check(SendMessageA(list,LVM_GETSELECTIONMARK,0,0)==0,"PASS selection mark");
  if(SendMessageA(list,LVM_SETCOLUMNORDERARRAY,2,(LPARAM)order)){
    check(SendMessageA(list,LVM_GETCOLUMNORDERARRAY,2,(LPARAM)readOrder)&&readOrder[0]==1&&readOrder[1]==0,"PASS column order round trip");
    ZeroMemory(&column,sizeof(column));column.mask=LVCF_ORDER;column.iOrder=-1;
    check(send(list,LVM_GETCOLUMNA,LVM_GETCOLUMNW,0,(LPARAM)&column)&&column.iOrder==1,"PASS column order available when saving view");
    column.iOrder=0;
    check(send(list,LVM_SETCOLUMNA,LVM_SETCOLUMNW,0,(LPARAM)&column),"PASS set individual column order");
    check(SendMessageA(list,LVM_GETCOLUMNORDERARRAY,2,(LPARAM)readOrder)&&readOrder[0]==0&&readOrder[1]==1,"PASS individual order preserves permutation");
    order[0]=0;order[1]=1;SendMessageA(list,LVM_SETCOLUMNORDERARRAY,2,(LPARAM)order);
  }else{check(!LegacyIsListView(list),"PASS column order required for fallback");line("SKIP native column order unavailable");}
  check(SendMessageA(list,LVM_SETCOLUMNWIDTH,0,LVSCW_AUTOSIZE),"PASS automatic column width");
  SendMessageA(list,LVM_SETCOLUMNWIDTH,0,1200);
  check(SendMessageA(list,LVM_ENSUREVISIBLE,0,FALSE),"PASS ensure wide report row");
  rect.left=LVIR_BOUNDS;SendMessageA(list,LVM_GETITEMRECT,0,(LPARAM)&rect);
  check(rect.left>=0,"PASS ensure report row preserves filename visibility");
  SendMessageA(list,LVM_SETCOLUMNWIDTH,0,180);SetFocus(list);
  state(list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);state(list,0,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
  SendMessageA(list,WM_KEYDOWN,VK_DOWN,0);check(SendMessageA(list,LVM_GETNEXTITEM,-1,LVNI_FOCUSED)==1,"PASS keyboard navigation");
  rect.left=LVIR_LABEL;check(SendMessageA(list,LVM_GETITEMRECT,2,(LPARAM)&rect),"PASS item rectangle");
  ZeroMemory(&hit,sizeof(hit));hit.pt.x=rect.left+2;hit.pt.y=(rect.top+rect.bottom)/2;
  check(SendMessageA(list,LVM_HITTEST,0,(LPARAM)&hit)==2&&(hit.flags&LVHT_ONITEM),"PASS hit test");
  ShowWindow(parent,SW_SHOW);SetActiveWindow(parent);UpdateWindow(parent);pump(50);clickAt(list,hit.pt);
  check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==1&&SendMessageA(list,LVM_GETNEXTITEM,-1,LVNI_SELECTED)==2,"PASS mouse selection");
  check(clicks>0,"PASS click notification");
  if(LegacyIsListView(list)){
    LPARAM point=MAKELPARAM(hit.pt.x,hit.pt.y);
    state(list,-1,LVIS_SELECTED,LVIS_SELECTED);
    SendMessageA(list,WM_LBUTTONDOWN,MK_LBUTTON,point);
    check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==3,"PASS selected-row mouse down retains multiple selection");
    SendMessageA(list,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(hit.pt.x+12,hit.pt.y+8));
    check(dragBegins==1&&dragSelection==3,"PASS drag callback receives every selected row");
    SendMessageA(list,WM_LBUTTONUP,0,point);
    check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==3,"PASS drag completion preserves selection");
    SendMessageA(list,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageA(list,WM_LBUTTONUP,0,point);
    check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==1,"PASS ordinary click collapses selection on release");
    state(list,-1,LVIS_SELECTED,LVIS_SELECTED);
    SendMessageA(list,WM_LBUTTONDOWN,MK_LBUTTON|MK_CONTROL,point);
    check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==3,"PASS Ctrl mouse down preserves potential drag selection");
    SendMessageA(list,WM_LBUTTONUP,MK_CONTROL,point);
    check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==2,"PASS Ctrl click deselects just the clicked row");
    state(list,-1,LVIS_SELECTED,LVIS_SELECTED);SendMessageA(list,WM_LBUTTONDOWN,MK_LBUTTON,point);ReleaseCapture();
    SendMessageA(list,WM_LBUTTONUP,0,point);
    check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==3,"PASS capture loss does not collapse selection");
  }
  SendMessageA(list,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(hit.pt.x,hit.pt.y));check(doubleClicks>0,"PASS double click notification");
  edit=(HWND)send(list,LVM_EDITLABELA,LVM_EDITLABELW,2,0);check(edit!=NULL&&begins>0,"PASS begin label edit");
  if(wide)SetWindowTextW(edit,L"Renamed");else SetWindowTextA(edit,"Renamed");
  SendMessageA(edit,WM_KEYDOWN,VK_RETURN,0);
  check(ends>0&&textIs(list,2,L"Renamed"),"PASS accepted label edit");
  edit=(HWND)send(list,LVM_EDITLABELA,LVM_EDITLABELW,2,0);check(edit!=NULL,"PASS second label edit");
  SendMessageA(edit,WM_KEYDOWN,VK_ESCAPE,0);check(textIs(list,2,L"Renamed"),"PASS cancelled label edit preserves text");
  if(LegacyIsListView(list)){
    POINT from,to;RECT first,last;state(list,-1,0,LVIS_SELECTED);
    first.left=last.left=LVIR_BOUNDS;SendMessageA(list,LVM_GETITEMRECT,0,(LPARAM)&first);SendMessageA(list,LVM_GETITEMRECT,2,(LPARAM)&last);
    from.x=30;from.y=last.bottom+12;to.x=100;to.y=first.top+2;
    SendMessageA(list,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(from.x,from.y));SendMessageA(list,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(to.x,to.y));
    check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==3,"PASS marquee selects intersected rows");
    SendMessageA(list,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(from.x,from.y));check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==0,"PASS shrinking marquee restores initial selection");
    SendMessageA(list,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(to.x,to.y));SendMessageA(list,WM_LBUTTONUP,0,MAKELPARAM(to.x,to.y));check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==3,"PASS marquee commits selection");
    SendMessageA(list,WM_LBUTTONDOWN,MK_LBUTTON|MK_CONTROL,MAKELPARAM(from.x,from.y));SendMessageA(list,WM_MOUSEMOVE,MK_LBUTTON|MK_CONTROL,MAKELPARAM(to.x,to.y));check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==0,"PASS control marquee toggles selection");SendMessageA(list,WM_KEYDOWN,VK_ESCAPE,0);check(SendMessageA(list,LVM_GETSELECTEDCOUNT,0,0)==3,"PASS escape restores pre-marquee selection");
  }
  for(i=3;i<103;i++)if(insert(list,i,L"Additional row",i+100)!=i)check(FALSE,"FAIL append row");
  check(SendMessageA(list,LVM_GETITEMCOUNT,0,0)==103,"PASS append 100 rows");
  check(SendMessageA(list,LVM_ENSUREVISIBLE,102,FALSE),"PASS scroll to final row");rect.left=LVIR_BOUNDS;SendMessageA(list,LVM_GETITEMRECT,102,(LPARAM)&rect);GetClientRect(list,&client);
  check(rect.top>=0&&rect.bottom<=client.bottom,"PASS final row visible");
  ShowWindow(parent,SW_SHOW);UpdateWindow(parent);UpdateWindow(list);check(callbacks>0&&changes>0,"PASS notifications delivered");
  check(SendMessageA(list,LVM_DELETEITEM,50,0)&&SendMessageA(list,LVM_GETITEMCOUNT,0,0)==102,"PASS delete item");
  check(SendMessageA(list,LVM_DELETEALLITEMS,0,0)&&SendMessageA(list,LVM_GETITEMCOUNT,0,0)==0,"PASS clear items");
  DestroyWindow(parent);line("PASS ALL");CloseHandle(report);ExitProcess(0);
}
