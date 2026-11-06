/* List-view control for NT 3.1, which predates SysListView32.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0500
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <commctrl.h>
#include <limits.h>

extern BOOL LegacyImageListDraw(HIMAGELIST,int,HDC,int,int);
#define MAX_ROWS 1048576
#define MAX_COLUMNS 4096
#define PROP_STATE "Z7.LegacyListView"

typedef struct Row {
  WCHAR **text;
  int cells, image;
  UINT state;
  LPARAM param;
} Row;
typedef struct Column { WCHAR *text; int width, format, subitem; } Column;
typedef struct View {
  HWND window, edit;
  Row **rows, *editRow;
  int count, capacity, columns, *order;
  Column *column;
  int mark, rowHeight, scrollX, scrollY, extentX, extentY;
  int downItem, dragColumn, dragX, dragWidth;
  POINT downPoint;
  RECT bandRect;
  BYTE *bandSelection;
  int bandCount;
  BOOL bandActive, bandControl, bandShift;
  BOOL unicode, redraw, closingEdit, dragged, layingOut;
  BOOL pendingSelection, pendingControl;
  HFONT font;
  DWORD extended;
  COLORREF background, textColor, textBackground;
  HIMAGELIST images[3];
  WNDPROC editProc;
  WCHAR search[128];
  unsigned searchLength;
  DWORD searchTick;
} View;
static LRESULT CALLBACK listProc(HWND,UINT,WPARAM,LPARAM);
static void *alloc(SIZE_T n) { return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,n); }
static void release(void *p) { if(p)HeapFree(GetProcessHeap(),0,p); }
static View *view(HWND w) { return (View*)GetPropA(w,PROP_STATE); }
BOOL LegacyIsListView(HWND w) { return view(w)!=NULL; }
static WCHAR *copyText(const void *s,BOOL wide)
{
  WCHAR *p; int n;
  if(!s || s==(void*)LPSTR_TEXTCALLBACKW)return (WCHAR*)s;
  n=wide?lstrlenW(s)+1:MultiByteToWideChar(CP_ACP,0,s,-1,NULL,0);
  if(n<=0)return NULL;
  p=alloc((SIZE_T)n*sizeof(WCHAR));
  if(p) { if(wide)CopyMemory(p,s,n*sizeof(WCHAR)); else MultiByteToWideChar(CP_ACP,0,s,-1,p,n); }
  return p;
}
static void freeText(WCHAR *s) { if(s!=LPSTR_TEXTCALLBACKW)release(s); }
static void freeRow(Row *r) { int n;for(n=0;n<r->cells;n++)freeText(r->text[n]);release(r->text);release(r); }
static LRESULT notify(View *v,NMHDR *h,UINT code)
{
  h->hwndFrom=v->window;h->idFrom=GetDlgCtrlID(v->window);h->code=code;
  return SendMessageW(GetParent(v->window),WM_NOTIFY,h->idFrom,(LPARAM)h);
}
static LRESULT itemNotify(View *v,UINT code,int index,int sub,UINT old,UINT state,UINT changed,POINT point)
{
  NMLISTVIEW n;ZeroMemory(&n,sizeof(n));n.iItem=index;n.iSubItem=sub;
  n.uOldState=old;n.uNewState=state;n.uChanged=changed;n.ptAction=point;
  if(index>=0&&index<v->count)n.lParam=v->rows[index]->param;
  return notify(v,&n.hdr,code);
}
static BOOL reserve(View *v,int count)
{
  Row **rows;int cap;
  if(count<0||count>MAX_ROWS)return FALSE;
  if(count<=v->capacity)return TRUE;
  cap=v->capacity?v->capacity*2:32;if(cap<count)cap=count;if(cap>MAX_ROWS)cap=MAX_ROWS;
  rows=alloc(cap*sizeof(*rows));if(!rows)return FALSE;
  if(v->count)CopyMemory(rows,v->rows,v->count*sizeof(*rows));release(v->rows);v->rows=rows;v->capacity=cap;return TRUE;
}
static BOOL putText(Row *r,int sub,const void *text,BOOL wide)
{
  WCHAR *s,**cells;
  if(sub<0||sub>=MAX_COLUMNS)return FALSE;
  s=copyText(text,wide);if(text&&!s)return FALSE;
  if(sub>=r->cells) {
    cells=alloc((sub+1)*sizeof(*cells));if(!cells){freeText(s);return FALSE;}
    if(r->cells)CopyMemory(cells,r->text,r->cells*sizeof(*cells));release(r->text);r->text=cells;r->cells=sub+1;
  }
  freeText(r->text[sub]);r->text[sub]=s;return TRUE;
}
static void display(View *v,int index,int sub,WCHAR *text,int capacity,int *image)
{
  Row *r;WCHAR *s;UINT mask=0;
  if(capacity>0)text[0]=0;
  if(index<0||index>=v->count)return;
  r=v->rows[index];s=sub<r->cells?r->text[sub]:LPSTR_TEXTCALLBACKW;
  if(image)*image=r->image;
  if(s==LPSTR_TEXTCALLBACKW)mask|=LVIF_TEXT;
  else if(s&&capacity>0)lstrcpynW(text,s,capacity);
  if(image&&r->image==I_IMAGECALLBACK)mask|=LVIF_IMAGE;
  if(!mask)return;
  if(v->unicode) {
    NMLVDISPINFOW n;ZeroMemory(&n,sizeof(n));n.item.mask=mask;n.item.iItem=index;n.item.iSubItem=sub;
    n.item.lParam=r->param;n.item.pszText=text;n.item.cchTextMax=capacity;n.item.iImage=r->image;
    notify(v,&n.hdr,LVN_GETDISPINFOW);
    if((mask&LVIF_TEXT)&&n.item.pszText&&n.item.pszText!=text&&n.item.pszText!=LPSTR_TEXTCALLBACKW&&capacity>0)
      lstrcpynW(text,n.item.pszText,capacity);
    if(image)*image=n.item.iImage;
  } else {
    NMLVDISPINFOA n;char *a;int bytes;
    if(capacity<1)return;
    bytes=capacity*2;a=alloc(bytes);if(!a)return;
    ZeroMemory(&n,sizeof(n));n.item.mask=mask;n.item.iItem=index;n.item.iSubItem=sub;
    n.item.lParam=r->param;n.item.pszText=a;n.item.cchTextMax=bytes;n.item.iImage=r->image;
    notify(v,&n.hdr,LVN_GETDISPINFOA);
    if((mask&LVIF_TEXT)&&n.item.pszText&&n.item.pszText!=LPSTR_TEXTCALLBACKA) {
      int needed=MultiByteToWideChar(CP_ACP,0,n.item.pszText,-1,NULL,0);
      WCHAR *buffer=needed>0?alloc(needed*sizeof(WCHAR)):NULL;
      if(buffer){MultiByteToWideChar(CP_ACP,0,n.item.pszText,-1,buffer,needed);lstrcpynW(text,buffer,capacity);release(buffer);}
    }
    if(image)*image=n.item.iImage;release(a);
  }
  if(capacity>0)text[capacity-1]=0;
}
static UINT mode(View *v) { return GetWindowLongW(v->window,GWL_STYLE)&LVS_TYPEMASK; }
static int headerHeight(View *v) { return mode(v)==LVS_REPORT&&!(GetWindowLongW(v->window,GWL_STYLE)&LVS_NOCOLUMNHEADER)?v->rowHeight+4:0; }
static void dimensions(View *v,int *width,int *height,int *across,int *down)
{
  RECT rc;UINT m=mode(v);GetClientRect(v->window,&rc);
  *width=m==LVS_ICON?96:160;*height=m==LVS_ICON?64:v->rowHeight;
  *across=rc.right/ *width;if(*across<1)*across=1;
  *down=(rc.bottom-headerHeight(v))/ *height;if(*down<1)*down=1;
}
static BOOL itemRect(View *v,int index,RECT *rc)
{
  int w,h,across,down,x=0,y=0;UINT m=mode(v);
  if(index<0||index>=v->count)return FALSE;
  dimensions(v,&w,&h,&across,&down);
  if(m==LVS_REPORT){GetClientRect(v->window,rc);w=v->extentX>rc->right?v->extentX:rc->right;y=index*h;}
  else if(m==LVS_LIST){x=(index/down)*w;y=(index%down)*h;}
  else{x=(index%across)*w;y=(index/across)*h;}
  SetRect(rc,x-v->scrollX,y-v->scrollY+headerHeight(v),x+w-v->scrollX,y+h-v->scrollY+headerHeight(v));return TRUE;
}
static void invalidate(View *v) { if(v->redraw)InvalidateRect(v->window,NULL,TRUE); }
static void layout(View *v)
{
  RECT rc;int w,h,across,down,i,maxX,maxY;UINT m=mode(v);
  if(v->layingOut)return;v->layingOut=TRUE;
  GetClientRect(v->window,&rc);dimensions(v,&w,&h,&across,&down);
  if(m==LVS_REPORT){v->extentX=0;for(i=0;i<v->columns;i++)v->extentX+=v->column[i].width;v->extentY=v->count*h;}
  else if(m==LVS_LIST){v->extentX=((v->count+down-1)/down)*w;v->extentY=down*h;}
  else{v->extentX=across*w;v->extentY=((v->count+across-1)/across)*h;}
  maxX=v->extentX-rc.right;maxY=v->extentY-(rc.bottom-headerHeight(v));if(maxX<0)maxX=0;if(maxY<0)maxY=0;
  if(v->scrollX>maxX)v->scrollX=maxX;if(v->scrollY>maxY)v->scrollY=maxY;
  if(v->scrollX<0)v->scrollX=0;if(v->scrollY<0)v->scrollY=0;
  SetScrollRange(v->window,SB_HORZ,0,maxX>32767?32767:maxX,FALSE);
  SetScrollRange(v->window,SB_VERT,0,maxY>32767?32767:maxY,FALSE);
  SetScrollPos(v->window,SB_HORZ,maxX>32767?MulDiv(v->scrollX,32767,maxX):v->scrollX,TRUE);
  SetScrollPos(v->window,SB_VERT,maxY>32767?MulDiv(v->scrollY,32767,maxY):v->scrollY,TRUE);
  v->layingOut=FALSE;
}
static BOOL ensure(View *v,int index)
{
  RECT r,c;if(!itemRect(v,index,&r))return FALSE;GetClientRect(v->window,&c);
  // A report row spans all columns. Scrolling to its right edge hides the
  // filename whenever the combined column width exceeds the viewport.
  if(mode(v)!=LVS_REPORT)
  {
    if(r.left<0)v->scrollX+=r.left;
    else if(r.right>c.right)v->scrollX+=r.right-c.right;
  }
  if(r.top<headerHeight(v))v->scrollY+=r.top-headerHeight(v);else if(r.bottom>c.bottom)v->scrollY+=r.bottom-c.bottom;
  layout(v);invalidate(v);return TRUE;
}
static BOOL stateOne(View *v,int index,UINT state,UINT mask)
{
  Row *r;UINT before,after;POINT pt={0,0};
  if(index<0||index>=v->count)return FALSE;r=v->rows[index];before=r->state;after=(before&~mask)|(state&mask);
  if(before==after)return TRUE;
  if(itemNotify(v,LVN_ITEMCHANGING,index,0,before,after,LVIF_STATE,pt))return FALSE;
  r->state=after;itemNotify(v,LVN_ITEMCHANGED,index,0,before,after,LVIF_STATE,pt);invalidate(v);return TRUE;
}
static BOOL setState(View *v,int index,UINT state,UINT mask)
{
  int i;BOOL ok=TRUE;
  if(index < -1 || index>=v->count)return FALSE;
  if(index==-1){for(i=0;i<v->count;i++)if(!stateOne(v,i,state,mask))ok=FALSE;return ok;}
  if((mask&state&LVIS_FOCUSED)||((GetWindowLongW(v->window,GWL_STYLE)&LVS_SINGLESEL)&&(mask&state&LVIS_SELECTED)))
    for(i=0;i<v->count;i++)if(i!=index)stateOne(v,i,0,mask&state&(LVIS_FOCUSED|((GetWindowLongW(v->window,GWL_STYLE)&LVS_SINGLESEL)?LVIS_SELECTED:0)));
  return stateOne(v,index,state,mask);
}
static int focused(View *v) { int i;for(i=0;i<v->count;i++)if(v->rows[i]->state&LVIS_FOCUSED)return i;return -1; }
static int hit(View *v,POINT p,UINT *flags,int *sub)
{
  RECT r,c;int i,x;GetClientRect(v->window,&c);*flags=LVHT_NOWHERE;if(sub)*sub=0;
  if(p.x<0){*flags=LVHT_TOLEFT;return -1;}if(p.x>=c.right){*flags=LVHT_TORIGHT;return -1;}
  if(p.y<headerHeight(v)||p.y>=c.bottom)return -1;
  for(i=0;i<v->count;i++)if(itemRect(v,i,&r)&&PtInRect(&r,p)){
    *flags=p.x<r.left+20?LVHT_ONITEMICON:LVHT_ONITEMLABEL;
    if(v->extended&LVS_EX_CHECKBOXES&&p.x<r.left+18)*flags=LVHT_ONITEMSTATEICON;
    if(sub&&mode(v)==LVS_REPORT){int j;x=-v->scrollX;for(j=0;j<v->columns;j++){int k=v->order[j];x+=v->column[k].width;if(p.x<x){*sub=k;break;}}}
    return i;
  }
  return -1;
}
static void choose(View *v,int index,BOOL control,BOOL shift)
{
  int i,from=v->mark,to=index;
  if(index<0){if(!control&&!shift)setState(v,-1,0,LVIS_SELECTED);return;}
  if(GetWindowLongW(v->window,GWL_STYLE)&LVS_SINGLESEL){control=FALSE;shift=FALSE;}
  if(shift&&from>=0&&from<v->count){if(from>to){int t=from;from=to;to=t;}for(i=0;i<v->count;i++)stateOne(v,i,i>=from&&i<=to?LVIS_SELECTED:0,LVIS_SELECTED);}
  else {if(!control)setState(v,-1,0,LVIS_SELECTED);stateOne(v,index,control?(v->rows[index]->state^LVIS_SELECTED):LVIS_SELECTED,LVIS_SELECTED);v->mark=index;}
  setState(v,index,LVIS_FOCUSED,LVIS_FOCUSED);ensure(v,index);
}
/* Keep the initial selection so shrinking a marquee restores each row. */
static void endBand(View *v)
{
  v->bandActive=FALSE;release(v->bandSelection);v->bandSelection=NULL;
  v->bandCount=0;invalidate(v);
}
static void updateBand(View *v,POINT point)
{
  int i;RECT r,overlap;
  v->bandRect.left=point.x<v->downPoint.x?point.x:v->downPoint.x;
  v->bandRect.right=(point.x>v->downPoint.x?point.x:v->downPoint.x)+1;
  v->bandRect.top=point.y<v->downPoint.y?point.y:v->downPoint.y;
  v->bandRect.bottom=(point.y>v->downPoint.y?point.y:v->downPoint.y)+1;
  for(i=0;i<v->count&&i<v->bandCount;i++){
    BOOL inside=itemRect(v,i,&r)&&IntersectRect(&overlap,&r,&v->bandRect);
    BOOL selected=v->bandControl?(v->bandSelection[i]!=inside):
      v->bandShift?(v->bandSelection[i]||inside):inside;
    stateOne(v,i,selected?LVIS_SELECTED:0,LVIS_SELECTED);
  }
  v->dragged=TRUE;invalidate(v);
}
static int nextItem(View *v,int start,UINT flags)
{
  UINT mask=0,direction=flags&(LVNI_ABOVE|LVNI_BELOW|LVNI_TOLEFT|LVNI_TORIGHT);
  int i,best=-1;long bestPrimary=LONG_MAX,bestSecondary=LONG_MAX;RECT origin,r;
  if(flags&LVNI_SELECTED)mask|=LVIS_SELECTED;if(flags&LVNI_FOCUSED)mask|=LVIS_FOCUSED;
  if(flags&LVNI_CUT)mask|=LVIS_CUT;if(flags&LVNI_DROPHILITED)mask|=LVIS_DROPHILITED;
  if(!direction){for(i=start+1;i<v->count;i++)if(i>=0&&(v->rows[i]->state&mask)==mask)return i;return -1;}
  if(!itemRect(v,start,&origin))return -1;
  for(i=0;i<v->count;i++)if(i!=start&&(v->rows[i]->state&mask)==mask&&itemRect(v,i,&r)){
    long dx=r.left-origin.left,dy=r.top-origin.top,primary,secondary;
    if(direction==LVNI_ABOVE){primary=-dy;secondary=dx;}
    else if(direction==LVNI_BELOW){primary=dy;secondary=dx;}
    else if(direction==LVNI_TOLEFT){primary=-dx;secondary=dy;}
    else if(direction==LVNI_TORIGHT){primary=dx;secondary=dy;}else return -1;
    if(primary<=0)continue;if(secondary<0)secondary=-secondary;
    if(secondary<bestSecondary||(secondary==bestSecondary&&primary<bestPrimary)){
      best=i;bestPrimary=primary;bestSecondary=secondary;
    }
  }
  return best;
}
static void fill(HDC dc,const RECT *r,COLORREF color) { HBRUSH b=CreateSolidBrush(color);if(b){FillRect(dc,r,b);DeleteObject(b);} }
static void paint(View *v)
{
  PAINTSTRUCT ps;HDC dc=BeginPaint(v->window,&ps);HFONT old=SelectObject(dc,v->font);RECT client,r;
  NMLVCUSTOMDRAW draw;LRESULT flags;int i,j;UINT m=mode(v);
  GetClientRect(v->window,&client);fill(dc,&ps.rcPaint,v->background);SetBkMode(dc,TRANSPARENT);
  ZeroMemory(&draw,sizeof(draw));draw.nmcd.hdc=dc;draw.nmcd.rc=client;draw.nmcd.dwDrawStage=CDDS_PREPAINT;
  flags=notify(v,&draw.nmcd.hdr,NM_CUSTOMDRAW);
  if(!(flags&CDRF_SKIPDEFAULT))for(i=0;i<v->count;i++){
    WCHAR text[1024];int icon,x;BOOL selected;
    if(!itemRect(v,i,&r)||r.bottom<=headerHeight(v)||r.top>=client.bottom||r.right<=0||r.left>=client.right)continue;
    selected=(v->rows[i]->state&LVIS_SELECTED)&&(GetFocus()==v->window||(GetWindowLongW(v->window,GWL_STYLE)&LVS_SHOWSELALWAYS));
    draw.nmcd.rc=r;draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=i;draw.nmcd.lItemlParam=v->rows[i]->param;
    draw.nmcd.uItemState=selected?CDIS_SELECTED:0;draw.clrText=selected?GetSysColor(COLOR_HIGHLIGHTTEXT):v->textColor;
    draw.clrTextBk=selected?GetSysColor(COLOR_HIGHLIGHT):(v->textBackground==CLR_NONE?v->background:v->textBackground);
    if((flags&CDRF_NOTIFYITEMDRAW)&&(notify(v,&draw.nmcd.hdr,NM_CUSTOMDRAW)&CDRF_SKIPDEFAULT))continue;
    fill(dc,&r,draw.clrTextBk);SetTextColor(dc,draw.clrText);x=r.left;
    for(j=0;j<(m==LVS_REPORT&&v->columns?v->columns:1);j++){
      int col=m==LVS_REPORT&&v->columns?v->order[j]:0;
      int sub=m==LVS_REPORT&&v->columns?v->column[col].subitem:0;
      RECT cell=r;UINT format=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX;
      if(m==LVS_REPORT&&v->columns){cell.left=x;cell.right=x+v->column[col].width;x=cell.right;
        if((v->column[col].format&LVCFMT_JUSTIFYMASK)==LVCFMT_RIGHT)format|=DT_RIGHT;
        else if((v->column[col].format&LVCFMT_JUSTIFYMASK)==LVCFMT_CENTER)format|=DT_CENTER;}
      display(v,i,sub,text,1024,&icon);cell.left+=3;cell.right-=3;
      if(col==0){
        if(v->extended&LVS_EX_CHECKBOXES){RECT box;HBRUSH b;SetRect(&box,cell.left,cell.top+3,cell.left+13,cell.top+16);b=GetStockObject(BLACK_BRUSH);FrameRect(dc,&box,b);if((v->rows[i]->state&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2)){MoveToEx(dc,box.left+2,box.top+6,NULL);LineTo(dc,box.left+5,box.bottom-3);LineTo(dc,box.right-2,box.top+2);}cell.left+=18;}
        if(icon>=0&&v->images[m==LVS_ICON?LVSIL_NORMAL:LVSIL_SMALL]){
          int ix=cell.left,iy=cell.top+2;if(m==LVS_ICON){ix=cell.left+(cell.right-cell.left-32)/2;iy=cell.top+3;}
          LegacyImageListDraw(v->images[m==LVS_ICON?LVSIL_NORMAL:LVSIL_SMALL],icon,dc,ix,iy);
        }
        if(m==LVS_ICON){cell.top+=36;format|=DT_CENTER;}else cell.left+=20;
      }
      DrawTextW(dc,text,-1,&cell,format);
    }
    if((v->rows[i]->state&LVIS_FOCUSED)&&GetFocus()==v->window)DrawFocusRect(dc,&r);
  }
  if(headerHeight(v)){
    int x=-v->scrollX;RECT h;fill(dc,&(RECT){0,0,client.right,headerHeight(v)},GetSysColor(COLOR_BTNFACE));
    SetTextColor(dc,GetSysColor(COLOR_BTNTEXT));
    for(i=0;i<v->columns;i++){int col=v->order[i];SetRect(&h,x,0,x+v->column[col].width,headerHeight(v));FrameRect(dc,&h,GetStockObject(GRAY_BRUSH));h.left+=4;h.right-=4;DrawTextW(dc,v->column[col].text?v->column[col].text:L"",-1,&h,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);x+=v->column[col].width;}
  }
  if(flags&CDRF_NOTIFYPOSTPAINT){draw.nmcd.dwDrawStage=CDDS_POSTPAINT;notify(v,&draw.nmcd.hdr,NM_CUSTOMDRAW);}
  if(v->bandActive&&v->dragged)DrawFocusRect(dc,&v->bandRect);
  SelectObject(dc,old);EndPaint(v->window,&ps);
}
static int findRow(View *v,Row *row) { int i;for(i=0;i<v->count;i++)if(v->rows[i]==row)return i;return -1; }
static void endEdit(View *v,BOOL commit)
{
  HWND edit=v->edit;Row *row=v->editRow;int index;WCHAR text[1024];BOOL accept=FALSE;
  if(!edit||v->closingEdit)return;v->closingEdit=TRUE;GetWindowTextW(edit,text,1024);index=findRow(v,row);
  if(index>=0){
    if(v->unicode){NMLVDISPINFOW n;ZeroMemory(&n,sizeof(n));n.item.iItem=index;n.item.lParam=row->param;n.item.pszText=commit?text:NULL;accept=notify(v,&n.hdr,LVN_ENDLABELEDITW)!=0;}
    else{NMLVDISPINFOA n;char a[2048];WideCharToMultiByte(CP_ACP,0,text,-1,a,sizeof(a),NULL,NULL);ZeroMemory(&n,sizeof(n));n.item.iItem=index;n.item.lParam=row->param;n.item.pszText=commit?a:NULL;accept=notify(v,&n.hdr,LVN_ENDLABELEDITA)!=0;}
    if(commit&&accept&&findRow(v,row)>=0)putText(row,0,text,TRUE);
  }
  v->edit=NULL;v->editRow=NULL;DestroyWindow(edit);v->closingEdit=FALSE;invalidate(v);
}
static LRESULT CALLBACK editProc(HWND w,UINT msg,WPARAM wp,LPARAM lp)
{
  View *v=view(GetParent(w));WNDPROC old=v?v->editProc:NULL;
  if(v&&msg==WM_KEYDOWN&&(wp==VK_RETURN||wp==VK_ESCAPE)){endEdit(v,wp==VK_RETURN);SetFocus(v->window);return 0;}
  if(msg==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
  return old?CallWindowProcW(old,w,msg,wp,lp):DefWindowProcW(w,msg,wp,lp);
}
static HWND editLabel(View *v,int index)
{
  RECT r;WCHAR text[1024];int icon;LRESULT veto;
  if(index<0||index>=v->count||!(GetWindowLongW(v->window,GWL_STYLE)&LVS_EDITLABELS))return NULL;
  endEdit(v,FALSE);ensure(v,index);itemRect(v,index,&r);display(v,index,0,text,1024,&icon);
  r.left+=23;if(mode(v)==LVS_REPORT&&v->columns)r.right=r.left+v->column[0].width-23;
  v->edit=CreateWindowExW(0,L"EDIT",text,WS_CHILD|WS_BORDER|ES_AUTOHSCROLL,r.left,r.top,r.right-r.left,v->rowHeight,v->window,(HMENU)1,GetModuleHandleA(NULL),NULL);
  if(!v->edit)return NULL;v->editRow=v->rows[index];SendMessageW(v->edit,WM_SETFONT,(WPARAM)v->font,FALSE);
  v->editProc=(WNDPROC)SetWindowLongW(v->edit,GWL_WNDPROC,(LONG)editProc);
  if(v->unicode){NMLVDISPINFOW n;ZeroMemory(&n,sizeof(n));n.item.iItem=index;n.item.lParam=v->editRow->param;n.item.pszText=text;veto=notify(v,&n.hdr,LVN_BEGINLABELEDITW);}
  else{NMLVDISPINFOA n;char a[2048];WideCharToMultiByte(CP_ACP,0,text,-1,a,sizeof(a),NULL,NULL);ZeroMemory(&n,sizeof(n));n.item.iItem=index;n.item.lParam=v->editRow->param;n.item.pszText=a;veto=notify(v,&n.hdr,LVN_BEGINLABELEDITA);}
  if(veto){HWND e=v->edit;v->edit=NULL;v->editRow=NULL;DestroyWindow(e);return NULL;}
  ShowWindow(v->edit,SW_SHOW);SetFocus(v->edit);SendMessageW(v->edit,EM_SETSEL,0,-1);return v->edit;
}
static BOOL sort(View *v,PFNLVCOMPARE compare,LPARAM context)
{
  Row **tmp,**from=v->rows,**to,*mark=v->mark>=0&&v->mark<v->count?v->rows[v->mark]:NULL;int width;
  if(!compare)return FALSE;if(v->count<2)return TRUE;tmp=alloc(v->count*sizeof(*tmp));if(!tmp)return FALSE;to=tmp;
  for(width=1;width<v->count;width*=2){int base;for(base=0;base<v->count;base+=2*width){int a=base,b=base+width,end=b+width,k=base,mid=b;if(mid>v->count)mid=v->count;if(end>v->count)end=v->count;
    while(a<mid||b<end){if(a<mid&&(b>=end||compare(from[a]->param,from[b]->param,context)<=0))to[k++]=from[a++];else to[k++]=from[b++];}}
    {Row **swap=from;from=to;to=swap;}}
  if(from!=v->rows)CopyMemory(v->rows,from,v->count*sizeof(*from));release(tmp);v->mark=findRow(v,mark);invalidate(v);return TRUE;
}
static int ansiCopy(char *out,int capacity,const WCHAR *text)
{
  int n,take=0;char *a;if(!out||capacity<=0)return 0;out[0]=0;
  n=WideCharToMultiByte(CP_ACP,0,text,-1,NULL,0,NULL,NULL);if(n<=0)return 0;
  a=alloc(n);if(!a)return 0;WideCharToMultiByte(CP_ACP,0,text,-1,a,n,NULL,NULL);
  while(a[take]){int bytes=IsDBCSLeadByte((BYTE)a[take])?2:1;if(take+bytes>=capacity)break;take+=bytes;}
  CopyMemory(out,a,take);out[take]=0;release(a);return take;
}
static BOOL getItem(View *v,LVITEMW *item,BOOL wide)
{
  WCHAR *text;int icon;
  if(!item||item->iItem<0||item->iItem>=v->count||item->iSubItem<0)return FALSE;
  if(item->mask&LVIF_PARAM)item->lParam=v->rows[item->iItem]->param;
  if(item->mask&LVIF_STATE)item->state=v->rows[item->iItem]->state&item->stateMask;
  if(item->mask&(LVIF_TEXT|LVIF_IMAGE)){
    int capacity=(item->mask&LVIF_TEXT)&&item->cchTextMax>0?item->cchTextMax:1;
    if(capacity>1048576)return FALSE;text=alloc(capacity*sizeof(WCHAR));if(!text)return FALSE;
    display(v,item->iItem,item->iSubItem,text,capacity,&icon);
    if((item->mask&LVIF_TEXT)&&item->pszText&&item->cchTextMax>0){if(wide)lstrcpynW(item->pszText,text,item->cchTextMax);else ansiCopy((char*)item->pszText,item->cchTextMax,text);}
    if(item->mask&LVIF_IMAGE)item->iImage=icon;release(text);
  }
  return TRUE;
}
static BOOL setItem(View *v,const LVITEMW *item,BOOL wide)
{
  Row *r;
  if(!item||item->iItem<0||item->iItem>=v->count||item->iSubItem<0)return FALSE;r=v->rows[item->iItem];
  if((item->mask&LVIF_TEXT)&&!putText(r,item->iSubItem,item->pszText,wide))return FALSE;
  if(item->mask&LVIF_PARAM)r->param=item->lParam;
  if(item->mask&LVIF_IMAGE)r->image=item->iImage;
  if(item->mask&LVIF_STATE)setState(v,item->iItem,item->state,item->stateMask);
  invalidate(v);return TRUE;
}
static int insertItem(View *v,const LVITEMW *item,BOOL wide)
{
  Row *row;int index,i;POINT pt={0,0};
  if(!item||item->iItem<0||item->iSubItem!=0||!reserve(v,v->count+1))return -1;
  if(v->bandActive)endBand(v);
  row=alloc(sizeof(*row));if(!row)return -1;row->image=0;
  if((item->mask&LVIF_TEXT)&&!putText(row,0,item->pszText,wide)){freeRow(row);return -1;}
  if(item->mask&LVIF_PARAM)row->param=item->lParam;
  if(item->mask&LVIF_IMAGE)row->image=item->iImage;
  if(v->extended&LVS_EX_CHECKBOXES)row->state=INDEXTOSTATEIMAGEMASK(1);
  index=item->iItem>v->count?v->count:item->iItem;
  for(i=v->count;i>index;i--)v->rows[i]=v->rows[i-1];v->rows[index]=row;v->count++;
  if(v->mark>=index)v->mark++;
  if(item->mask&LVIF_STATE)setState(v,index,item->state,item->stateMask);
  itemNotify(v,LVN_INSERTITEM,index,0,0,row->state,0,pt);layout(v);invalidate(v);return index;
}
static BOOL deleteItem(View *v,int index,BOOL sendNotification)
{
  int i;POINT pt={0,0};Row *row;
  if(index<0||index>=v->count)return FALSE;
  if(v->bandActive)endBand(v);
  row=v->rows[index];if(v->editRow==row)endEdit(v,FALSE);
  if(sendNotification)itemNotify(v,LVN_DELETEITEM,index,0,0,0,0,pt);
  freeRow(row);for(i=index;i<v->count-1;i++)v->rows[i]=v->rows[i+1];v->count--;
  if(v->mark==index)v->mark=-1;else if(v->mark>index)v->mark--;
  return TRUE;
}
static int columnMessage(View *v,UINT message,int index,LVCOLUMNW *item,BOOL wide)
{
  BOOL insert=message==LVM_INSERTCOLUMNW||message==LVM_INSERTCOLUMNA;
  BOOL get=message==LVM_GETCOLUMNW||message==LVM_GETCOLUMNA;
  Column *c;WCHAR *newText=NULL;
  if(!item||index<0||(!insert&&index>=v->columns))return insert?-1:FALSE;
  if(!get&&(item->mask&LVCF_TEXT)){
    newText=copyText(item->pszText,wide);
    if(item->pszText&&!newText)return insert?-1:FALSE;
  }
  if(insert){
    Column *cols;int *order,i;if(v->columns>=MAX_COLUMNS){freeText(newText);return -1;}if(index>v->columns)index=v->columns;
    cols=alloc((v->columns+1)*sizeof(*cols));order=alloc((v->columns+1)*sizeof(*order));
    if(!cols||!order){release(cols);release(order);freeText(newText);return -1;}
    for(i=0;i<v->columns;i++)cols[i<index?i:i+1]=v->column[i];
    for(i=0;i<index;i++)order[i]=v->order[i]+(v->order[i]>=index);
    order[index]=index;for(i=index;i<v->columns;i++)order[i+1]=v->order[i]+(v->order[i]>=index);
    release(v->column);release(v->order);v->column=cols;v->order=order;v->columns++;
    cols[index].width=100;cols[index].subitem=index;
  }
  c=&v->column[index];
  if(get){
    if(item->mask&LVCF_ORDER){int i;for(i=0;i<v->columns;i++)if(v->order[i]==index){item->iOrder=i;break;}}
    if(item->mask&LVCF_WIDTH)item->cx=c->width;
    if(item->mask&LVCF_FMT)item->fmt=c->format;
    if(item->mask&LVCF_SUBITEM)item->iSubItem=c->subitem;
    if(item->mask&LVCF_TEXT&&item->pszText&&item->cchTextMax>0){if(wide)lstrcpynW(item->pszText,c->text?c->text:L"",item->cchTextMax);else ansiCopy((char*)item->pszText,item->cchTextMax,c->text?c->text:L"");}
    return TRUE;
  }
  if(item->mask&LVCF_TEXT){freeText(c->text);c->text=newText;}
  if(item->mask&LVCF_WIDTH)c->width=item->cx<0?0:item->cx;
  if(item->mask&LVCF_FMT)c->format=item->fmt;
  if(item->mask&LVCF_SUBITEM)c->subitem=item->iSubItem;
  if(item->mask&LVCF_ORDER){
    int from,to=item->iOrder,i;
    if(to<0)to=0;if(to>=v->columns)to=v->columns-1;
    for(from=0;from<v->columns;from++)if(v->order[from]==index)break;
    if(from<to)for(i=from;i<to;i++)v->order[i]=v->order[i+1];
    else for(i=from;i>to;i--)v->order[i]=v->order[i-1];
    v->order[to]=index;
  }
  layout(v);invalidate(v);return insert?index:TRUE;
}
static int textWidth(View *v,const WCHAR *text)
{
  HDC dc=GetDC(v->window);HFONT old;SIZE size={0,0};if(!dc)return 0;
  old=SelectObject(dc,v->font);GetTextExtentPointW(dc,text,lstrlenW(text),&size);SelectObject(dc,old);ReleaseDC(v->window,dc);return size.cx;
}
static BOOL columnWidth(View *v,int index,int width)
{
  int i,max=0;if(index<0||index>=v->columns)return FALSE;
  if(width==LVSCW_AUTOSIZE||width==LVSCW_AUTOSIZE_USEHEADER){
    WCHAR text[1024];for(i=0;i<v->count;i++){int n;display(v,i,v->column[index].subitem,text,1024,NULL);n=textWidth(v,text)+10+(index==0?20:0);if(n>max)max=n;}
    if(width==LVSCW_AUTOSIZE_USEHEADER){int n=textWidth(v,v->column[index].text?v->column[index].text:L"")+12;if(n>max)max=n;}
    width=max;
  }
  if(width<0)return FALSE;v->column[index].width=width;layout(v);invalidate(v);return TRUE;
}
static void scroll(View *v,BOOL horizontal,int command,int thumb)
{
  RECT rc;int pos,max,line,page;GetClientRect(v->window,&rc);
  pos=horizontal?v->scrollX:v->scrollY;line=horizontal?20:v->rowHeight;page=horizontal?rc.right:rc.bottom-headerHeight(v);
  max=(horizontal?v->extentX:v->extentY)-page;if(max<0)max=0;
  switch(command){case SB_LINEUP:pos-=line;break;case SB_LINEDOWN:pos+=line;break;
    case SB_PAGEUP:pos-=page;break;case SB_PAGEDOWN:pos+=page;break;case SB_TOP:pos=0;break;case SB_BOTTOM:pos=max;break;
    case SB_THUMBTRACK:case SB_THUMBPOSITION:pos=max>32767?MulDiv(thumb,max,32767):thumb;break;default:return;}
  if(pos<0)pos=0;if(pos>max)pos=max;if(horizontal)v->scrollX=pos;else v->scrollY=pos;layout(v);invalidate(v);
}
static void key(View *v,WPARAM value)
{
  NMLVKEYDOWN n;int index,w,h,across,down,delta=1;BOOL ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0,shift=(GetKeyState(VK_SHIFT)&0x8000)!=0;POINT pt={0,0};
  ZeroMemory(&n,sizeof(n));n.wVKey=(WORD)value;if(notify(v,&n.hdr,LVN_KEYDOWN))return;
  index=focused(v);dimensions(v,&w,&h,&across,&down);
  if(value==VK_RETURN){itemNotify(v,NM_RETURN,index,0,0,0,0,pt);return;}
  if(value==VK_F2){editLabel(v,index);return;}
  if(value=='A'&&ctrl&&!(GetWindowLongW(v->window,GWL_STYLE)&LVS_SINGLESEL)){setState(v,-1,LVIS_SELECTED,LVIS_SELECTED);return;}
  if(value==VK_SPACE&&index>=0){if(v->extended&LVS_EX_CHECKBOXES)setState(v,index,(v->rows[index]->state&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2)?INDEXTOSTATEIMAGEMASK(1):INDEXTOSTATEIMAGEMASK(2),LVIS_STATEIMAGEMASK);else choose(v,index,ctrl,shift);return;}
  if(!v->count)return;if(index<0)index=0;
  switch(value){case VK_HOME:index=0;break;case VK_END:index=v->count-1;break;
    case VK_PRIOR:index-=down;break;case VK_NEXT:index+=down;break;
    case VK_UP:delta=mode(v)==LVS_ICON||mode(v)==LVS_SMALLICON?across:1;index-=delta;break;
    case VK_DOWN:delta=mode(v)==LVS_ICON||mode(v)==LVS_SMALLICON?across:1;index+=delta;break;
    case VK_LEFT:if(mode(v)==LVS_REPORT){scroll(v,TRUE,SB_LINEUP,0);return;}index-=mode(v)==LVS_LIST?down:1;break;
    case VK_RIGHT:if(mode(v)==LVS_REPORT){scroll(v,TRUE,SB_LINEDOWN,0);return;}index+=mode(v)==LVS_LIST?down:1;break;
    default:return;}
  if(index<0)index=0;if(index>=v->count)index=v->count-1;
  if(ctrl&&!shift){setState(v,index,LVIS_FOCUSED,LVIS_FOCUSED);ensure(v,index);}else choose(v,index,FALSE,shift);
}
static void search(View *v,WCHAR c)
{
  DWORD tick=GetTickCount();int index=focused(v),i;WCHAR text[1024];
  if(c<32||(GetKeyState(VK_CONTROL)&0x8000))return;
  if(tick-v->searchTick>1000)v->searchLength=0;
  v->searchTick=tick;
  if(v->searchLength==1&&v->search[0]==c)v->searchLength=0;
  if(v->searchLength>=127)v->searchLength=0;v->search[v->searchLength++]=c;v->search[v->searchLength]=0;
  for(i=0;i<v->count;i++){
    int candidate=(index+1+i)%v->count;unsigned j;display(v,candidate,0,text,1024,NULL);
    if(lstrlenW(text)<(int)v->searchLength)continue;
    for(j=0;j<v->searchLength;j++){WCHAR a[2]={text[j],0},b[2]={v->search[j],0};if(lstrcmpiW(a,b))break;}
    if(j==v->searchLength){choose(v,candidate,FALSE,FALSE);break;}
  }
}
static LRESULT CALLBACK listProc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
  View *v=view(window);int i;POINT point={(short)LOWORD(lp),(short)HIWORD(lp)};
  if(message==WM_CREATE){
    if(GetWindowLongW(window,GWL_STYLE)&LVS_OWNERDATA){SetLastError(ERROR_NOT_SUPPORTED);return -1;}
    v=alloc(sizeof(*v));if(!v)return -1;v->window=window;v->mark=-1;v->downItem=-1;v->dragColumn=-1;
    v->rowHeight=20;v->unicode=TRUE;v->redraw=TRUE;v->font=GetStockObject(SYSTEM_FONT);
    v->background=GetSysColor(COLOR_WINDOW);v->textColor=GetSysColor(COLOR_WINDOWTEXT);v->textBackground=CLR_NONE;
    if(!SetPropA(window,PROP_STATE,(HANDLE)v)){release(v);return -1;}return 0;
  }
  if(!v)return DefWindowProcW(window,message,wp,lp);
  switch(message){
  case WM_NCDESTROY:
    RemovePropA(window,PROP_STATE);for(i=0;i<v->count;i++)freeRow(v->rows[i]);release(v->rows);
    for(i=0;i<v->columns;i++)freeText(v->column[i].text);release(v->column);release(v->order);
    if(!(GetWindowLongW(window,GWL_STYLE)&LVS_SHAREIMAGELISTS))for(i=0;i<3;i++)if(v->images[i])ImageList_Destroy(v->images[i]);
    release(v->bandSelection);release(v);return DefWindowProcW(window,message,wp,lp);
  case WM_PAINT:paint(v);return 0;
  case WM_ERASEBKGND:return 1;
  case WM_SIZE:layout(v);invalidate(v);return 0;
  case WM_SETREDRAW:v->redraw=(BOOL)wp;if(v->redraw){layout(v);invalidate(v);}return 0;
  case WM_SETFONT:{HDC dc;TEXTMETRICW tm;HFONT old;v->font=wp?(HFONT)wp:GetStockObject(SYSTEM_FONT);dc=GetDC(window);old=SelectObject(dc,v->font);if(GetTextMetricsW(dc,&tm))v->rowHeight=tm.tmHeight+4;if(v->rowHeight<20)v->rowHeight=20;SelectObject(dc,old);ReleaseDC(window,dc);layout(v);if(lp)invalidate(v);return 0;}
  case WM_GETFONT:return (LRESULT)v->font;
  case WM_GETDLGCODE:return DLGC_WANTARROWS|DLGC_WANTCHARS;
  case WM_SETFOCUS:case WM_KILLFOCUS:{NMHDR n;invalidate(v);notify(v,&n,message==WM_SETFOCUS?NM_SETFOCUS:NM_KILLFOCUS);return 0;}
  case WM_COMMAND:if((HWND)lp==v->edit&&HIWORD(wp)==EN_KILLFOCUS)endEdit(v,TRUE);return 0;
  case WM_KEYDOWN:if(wp==VK_ESCAPE&&v->bandActive){for(i=0;i<v->count&&i<v->bandCount;i++)stateOne(v,i,v->bandSelection[i]?LVIS_SELECTED:0,LVIS_SELECTED);endBand(v);if(GetCapture()==window)ReleaseCapture();return 0;}key(v,wp);return 0;
  case WM_CAPTURECHANGED:if(v->bandActive)endBand(v);v->dragColumn=-1;v->pendingSelection=FALSE;return 0;
  case WM_CHAR:search(v,(WCHAR)wp);return 0;
  case WM_HSCROLL:scroll(v,TRUE,LOWORD(wp),HIWORD(wp));return 0;
  case WM_VSCROLL:scroll(v,FALSE,LOWORD(wp),HIWORD(wp));return 0;
  case WM_MOUSEWHEEL:v->scrollY-=((short)HIWORD(wp)/WHEEL_DELTA)*3*v->rowHeight;layout(v);invalidate(v);return 0;
  case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:{UINT flags;int sub=0,index;SetFocus(window);endEdit(v,TRUE);v->dragged=FALSE;v->pendingSelection=FALSE;v->downPoint=point;
    if(message==WM_LBUTTONDOWN&&point.y<headerHeight(v)){int x=-v->scrollX;for(i=0;i<v->columns;i++){int col=v->order[i];x+=v->column[col].width;if(point.x>=x-3&&point.x<=x+3){v->dragColumn=col;v->dragX=point.x;v->dragWidth=v->column[col].width;SetCapture(window);return 0;}if(point.x<x){itemNotify(v,LVN_COLUMNCLICK,-1,col,0,0,0,point);return 0;}}return 0;}
    index=hit(v,point,&flags,&sub);v->downItem=index;
    if(message==WM_LBUTTONDOWN&&index<0&&point.y>=headerHeight(v)&&!(GetWindowLongW(window,GWL_STYLE)&LVS_SINGLESEL)){
      endBand(v);v->bandSelection=alloc(v->count?v->count:1);
      if(v->bandSelection){v->bandCount=v->count;for(i=0;i<v->count;i++)v->bandSelection[i]=(v->rows[i]->state&LVIS_SELECTED)!=0;v->bandActive=TRUE;v->bandControl=(wp&MK_CONTROL)!=0;v->bandShift=(wp&MK_SHIFT)!=0;SetRectEmpty(&v->bandRect);}
    }
    if(message==WM_LBUTTONDOWN&&index>=0&&(flags&LVHT_ONITEMSTATEICON))setState(v,index,(v->rows[index]->state&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2)?INDEXTOSTATEIMAGEMASK(1):INDEXTOSTATEIMAGEMASK(2),LVIS_STATEIMAGEMASK);
    /* Clicking an already-selected row must retain the whole selection
       until we know whether this is a click or a drag. */
    if(message==WM_LBUTTONDOWN&&index>=0&&(v->rows[index]->state&LVIS_SELECTED)
       &&!(wp&MK_SHIFT)&&!(flags&LVHT_ONITEMSTATEICON)){
      v->pendingSelection=TRUE;v->pendingControl=(wp&MK_CONTROL)!=0;
      setState(v,index,LVIS_FOCUSED,LVIS_FOCUSED);ensure(v,index);
    }else if(message==WM_LBUTTONDOWN||index<0||!(v->rows[index]->state&LVIS_SELECTED))
      choose(v,index,(wp&MK_CONTROL)!=0,(wp&MK_SHIFT)!=0);
    SetCapture(window);return 0;}
  case WM_MOUSEMOVE:
    if(v->bandActive&&GetCapture()==window){updateBand(v,point);return 0;}
    if(v->dragColumn>=0){int width=v->dragWidth+point.x-v->dragX;if(width<8)width=8;columnWidth(v,v->dragColumn,width);return 0;}
    if(GetCapture()==window&&!v->dragged&&v->downItem>=0&&((point.x-v->downPoint.x>4)||(point.x-v->downPoint.x < -4)||(point.y-v->downPoint.y>4)||(point.y-v->downPoint.y < -4))){v->dragged=TRUE;ReleaseCapture();itemNotify(v,(wp&MK_RBUTTON)?LVN_BEGINRDRAG:LVN_BEGINDRAG,v->downItem,0,0,0,0,v->downPoint);}return 0;
  case WM_LBUTTONUP:case WM_RBUTTONUP:{
    BOOL pending=v->pendingSelection&&GetCapture()==window,control=v->pendingControl;v->pendingSelection=FALSE;
    if(v->bandActive){endBand(v);if(GetCapture()==window)ReleaseCapture();v->downItem=-1;return 0;}
    if(v->dragColumn>=0){v->dragColumn=-1;if(GetCapture()==window)ReleaseCapture();return 0;}if(GetCapture()==window)ReleaseCapture();
    if(!v->dragged){UINT flags;int sub,index=hit(v,point,&flags,&sub);
      if(pending&&index==v->downItem)choose(v,index,control,FALSE);
      itemNotify(v,message==WM_LBUTTONUP?NM_CLICK:NM_RCLICK,index,sub,0,0,0,point);
      if(message==WM_RBUTTONUP){POINT screen=point;ClientToScreen(window,&screen);SendMessageW(window,WM_CONTEXTMENU,(WPARAM)window,MAKELPARAM(screen.x,screen.y));}}
    v->downItem=-1;return 0;}
  case WM_LBUTTONDBLCLK:{UINT flags;int sub,index=hit(v,point,&flags,&sub);itemNotify(v,NM_DBLCLK,index,sub,0,0,0,point);return 0;}
  case CCM_SETUNICODEFORMAT:{BOOL before=v->unicode;v->unicode=(BOOL)wp;return before;}
  case CCM_GETUNICODEFORMAT:return v->unicode;
  case LVM_INSERTITEMA:case LVM_INSERTITEMW:return insertItem(v,(const LVITEMW*)lp,message==LVM_INSERTITEMW);
  case LVM_SETITEMA:case LVM_SETITEMW:return setItem(v,(const LVITEMW*)lp,message==LVM_SETITEMW);
  case LVM_GETITEMA:case LVM_GETITEMW:return getItem(v,(LVITEMW*)lp,message==LVM_GETITEMW);
  case LVM_SETITEMTEXTA:case LVM_SETITEMTEXTW:{const LVITEMW *p=(const LVITEMW*)lp;LVITEMW item;ZeroMemory(&item,sizeof(item));if(!p)return FALSE;item.mask=LVIF_TEXT;item.iItem=(int)wp;item.iSubItem=p->iSubItem;item.pszText=p->pszText;return setItem(v,&item,message==LVM_SETITEMTEXTW);}
  case LVM_GETITEMTEXTA:case LVM_GETITEMTEXTW:{LVITEMW *p=(LVITEMW*)lp,item;BOOL ok;if(!p)return 0;ZeroMemory(&item,sizeof(item));item.mask=LVIF_TEXT;item.iItem=(int)wp;item.iSubItem=p->iSubItem;item.pszText=p->pszText;item.cchTextMax=p->cchTextMax;ok=getItem(v,&item,message==LVM_GETITEMTEXTW);return ok&&item.pszText&&item.cchTextMax>0?(message==LVM_GETITEMTEXTW?lstrlenW(item.pszText):lstrlenA((char*)item.pszText)):0;}
  case LVM_GETITEMCOUNT:return v->count;
  case LVM_SETITEMCOUNT:return reserve(v,(int)wp);
  case LVM_DELETEITEM:{BOOL result=deleteItem(v,(int)wp,TRUE);layout(v);invalidate(v);return result;}
  case LVM_DELETEALLITEMS:{NMHDR n;BOOL individual=notify(v,&n,LVN_DELETEALLITEMS)==0;endEdit(v,FALSE);while(v->count)deleteItem(v,v->count-1,individual);layout(v);invalidate(v);return TRUE;}
  case LVM_GETSELECTEDCOUNT:{int count=0;for(i=0;i<v->count;i++)if(v->rows[i]->state&LVIS_SELECTED)count++;return count;}
  case LVM_GETNEXTITEM:return nextItem(v,(int)wp,(UINT)lp);
  case LVM_SETITEMSTATE:{const LVITEMW *item=(const LVITEMW*)lp;return item?setState(v,(int)wp,item->state,item->stateMask):FALSE;}
  case LVM_GETITEMSTATE:return (int)wp>=0&&(int)wp<v->count?v->rows[wp]->state&(UINT)lp:0;
  case LVM_GETSELECTIONMARK:return v->mark;
  case LVM_SETSELECTIONMARK:{int old=v->mark;v->mark=(int)lp;return old;}
  case LVM_SORTITEMS:return sort(v,(PFNLVCOMPARE)lp,(LPARAM)wp);
  case LVM_SETIMAGELIST:{HIMAGELIST old;if(wp>2)return 0;old=v->images[wp];v->images[wp]=(HIMAGELIST)lp;invalidate(v);return (LRESULT)old;}
  case LVM_GETIMAGELIST:return wp<=2?(LRESULT)v->images[wp]:0;
  case LVM_SETEXTENDEDLISTVIEWSTYLE:{DWORD old=v->extended;v->extended=wp?(old&~wp)|(lp&wp):(DWORD)lp;if(!(old&LVS_EX_CHECKBOXES)&&(v->extended&LVS_EX_CHECKBOXES))for(i=0;i<v->count;i++)v->rows[i]->state=(v->rows[i]->state&~LVIS_STATEIMAGEMASK)|INDEXTOSTATEIMAGEMASK(1);invalidate(v);return old;}
  case LVM_GETEXTENDEDLISTVIEWSTYLE:return v->extended;
  case LVM_ENSUREVISIBLE:return ensure(v,(int)wp);
  case LVM_GETITEMRECT:{RECT *r=(RECT*)lp;int code;if(!r)return FALSE;code=r->left;if(!itemRect(v,(int)wp,r))return FALSE;if(code==LVIR_ICON){r->left+=3;r->right=r->left+(mode(v)==LVS_ICON?32:16);r->bottom=r->top+(mode(v)==LVS_ICON?32:16);}else if(code==LVIR_LABEL)r->left+=23;return TRUE;}
  case LVM_HITTEST:case LVM_SUBITEMHITTEST:{LVHITTESTINFO *h=(LVHITTESTINFO*)lp;int sub;if(!h)return -1;h->iItem=hit(v,h->pt,&h->flags,&sub);if(message==LVM_SUBITEMHITTEST)h->iSubItem=sub;return h->iItem;}
  case LVM_REDRAWITEMS:invalidate(v);return TRUE;
  case LVM_GETBKCOLOR:return v->background;
  case LVM_SETBKCOLOR:v->background=(COLORREF)lp;invalidate(v);return TRUE;
  case LVM_GETTEXTCOLOR:return v->textColor;
  case LVM_SETTEXTCOLOR:v->textColor=(COLORREF)lp;invalidate(v);return TRUE;
  case LVM_GETTEXTBKCOLOR:return v->textBackground;
  case LVM_SETTEXTBKCOLOR:v->textBackground=(COLORREF)lp;invalidate(v);return TRUE;
  case LVM_INSERTCOLUMNA:case LVM_INSERTCOLUMNW:case LVM_SETCOLUMNA:case LVM_SETCOLUMNW:case LVM_GETCOLUMNA:case LVM_GETCOLUMNW:
    return columnMessage(v,message,(int)wp,(LVCOLUMNW*)lp,message==LVM_INSERTCOLUMNW||message==LVM_SETCOLUMNW||message==LVM_GETCOLUMNW);
  case LVM_DELETECOLUMN:
    if((int)wp<0||(int)wp>=v->columns)return FALSE;freeText(v->column[wp].text);
    for(i=(int)wp;i<v->columns-1;i++)v->column[i]=v->column[i+1];
    {int n=0;for(i=0;i<v->columns;i++)if(v->order[i]!=(int)wp)v->order[n++]=v->order[i]-(v->order[i]>(int)wp);}
    v->columns--;layout(v);invalidate(v);return TRUE;
  case LVM_SETCOLUMNWIDTH:return columnWidth(v,(int)wp,(short)LOWORD(lp));
  case LVM_GETCOLUMNWIDTH:return (int)wp>=0&&(int)wp<v->columns?v->column[wp].width:0;
  case LVM_SETCOLUMNORDERARRAY:{int *order=(int*)lp,j;if((int)wp!=v->columns||(!order&&wp))return FALSE;for(i=0;i<v->columns;i++){if(order[i]<0||order[i]>=v->columns)return FALSE;for(j=0;j<i;j++)if(order[j]==order[i])return FALSE;}if(wp)CopyMemory(v->order,order,wp*sizeof(int));invalidate(v);return TRUE;}
  case LVM_GETCOLUMNORDERARRAY:if((int)wp!=v->columns||(!lp&&wp))return FALSE;if(wp)CopyMemory((void*)lp,v->order,wp*sizeof(int));return TRUE;
  case LVM_GETEDITCONTROL:return (LRESULT)v->edit;
  case LVM_EDITLABELA:case LVM_EDITLABELW:return (LRESULT)editLabel(v,(int)wp);
  case LVM_CANCELEDITLABEL:endEdit(v,FALSE);return TRUE;
  case LVM_GETSTRINGWIDTHA:case LVM_GETSTRINGWIDTHW:{WCHAR *s=copyText((void*)lp,message==LVM_GETSTRINGWIDTHW);int n=s?textWidth(v,s):0;freeText(s);return n;}
  case LVM_SCROLL:v->scrollX+=(int)wp;v->scrollY+=(int)lp;layout(v);invalidate(v);return TRUE;
  case LVM_GETTOPINDEX:{int w,h,across,down;dimensions(v,&w,&h,&across,&down);return mode(v)==LVS_REPORT?v->scrollY/h:mode(v)==LVS_LIST?(v->scrollX/w)*down:(v->scrollY/h)*across;}
  case LVM_GETCOUNTPERPAGE:{int w,h,across,down;dimensions(v,&w,&h,&across,&down);return mode(v)==LVS_REPORT?down:across*down;}
  }
  if(message>=LVM_FIRST&&message<LVM_FIRST+256){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return 0;}
  return DefWindowProcW(window,message,wp,lp);
}
BOOL LegacyRegisterListView(void)
{
  WNDCLASSW wc;WNDCLASSA existing;HINSTANCE instance=GetModuleHandleA(NULL);
  if(GetClassInfoA(instance,WC_LISTVIEWA,&existing)||GetClassInfoA(NULL,WC_LISTVIEWA,&existing))return TRUE;
  if(GetClassInfoW(instance,WC_LISTVIEWW,&wc)||GetClassInfoW(NULL,WC_LISTVIEWW,&wc))return TRUE;
  ZeroMemory(&wc,sizeof(wc));wc.style=CS_DBLCLKS|CS_GLOBALCLASS;wc.lpfnWndProc=listProc;wc.hInstance=instance;
  wc.hCursor=LoadCursorA(NULL,IDC_ARROW);wc.lpszClassName=WC_LISTVIEWW;
  return RegisterClassW(&wc)!=0;
}
