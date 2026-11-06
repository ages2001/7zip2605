/* Report-view rendering for the native Win16 file list: per-row file/folder
   glyphs drawn with GDI primitives (no bitmap resources), owner-drawn aligned
   columns, and the column-header bar.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_LIST_RENDER_H
#define Z7_WIN16_LIST_RENDER_H
#ifdef __WATCOMC__
#pragma code_seg("LISTDRAW_TEXT")
#endif
/* A 16x16 folder (kind 1), parent-folder-with-up-arrow (kind 2) or document
   (kind 0) glyph at (x,y). Colours are fixed so the icons read on any palette. */
static void drawFileGlyph(HDC dc,int x,int y,int kind){
 HPEN pen,oldPen;HBRUSH br,oldBr;
 if(kind){
  pen=CreatePen(PS_SOLID,1,RGB(123,97,0));br=CreateSolidBrush(RGB(255,221,102));
  oldPen=(HPEN)SelectObject(dc,pen);oldBr=(HBRUSH)SelectObject(dc,br);
  Rectangle(dc,x+1,y+3,x+8,y+6);
  Rectangle(dc,x+1,y+5,x+15,y+14);
  SelectObject(dc,oldBr);SelectObject(dc,oldPen);DeleteObject(br);DeleteObject(pen);
  if(kind==Z7_ITEM_UP){
   HPEN ap=CreatePen(PS_SOLID,1,RGB(0,0,160));HBRUSH ab=CreateSolidBrush(RGB(0,0,160));POINT p[3];
   oldPen=(HPEN)SelectObject(dc,ap);oldBr=(HBRUSH)SelectObject(dc,ab);
   p[0].x=x+8;p[0].y=y+6;p[1].x=x+4;p[1].y=y+10;p[2].x=x+12;p[2].y=y+10;Polygon(dc,p,3);
   Rectangle(dc,x+7,y+9,x+9,y+13);
   SelectObject(dc,oldBr);SelectObject(dc,oldPen);DeleteObject(ab);DeleteObject(ap);
  }
 } else {
  pen=CreatePen(PS_SOLID,1,RGB(96,96,96));br=CreateSolidBrush(RGB(255,255,255));
  oldPen=(HPEN)SelectObject(dc,pen);oldBr=(HBRUSH)SelectObject(dc,br);
  Rectangle(dc,x+2,y+1,x+13,y+15);
  MoveTo(dc,x+4,y+4);LineTo(dc,x+11,y+4);
  MoveTo(dc,x+4,y+7);LineTo(dc,x+11,y+7);
  MoveTo(dc,x+4,y+10);LineTo(dc,x+9,y+10);
  SelectObject(dc,oldBr);SelectObject(dc,oldPen);DeleteObject(br);DeleteObject(pen);
 }
}
/* Owner-draw one list row: selection background, glyph, then the Name / Size /
   Modified / Attr columns parsed from the tab-delimited item text. */
/* Right-align text so it ends at column edge x=right (used for the Size value). */
static void textRight(HDC dc,int right,int y,const char *s){
 int len=lstrlen(s);DWORD e;if(!len)return;e=GetTextExtent(dc,(LPSTR)s,len);TextOut(dc,right-(int)LOWORD(e),y,(LPSTR)s,len);
}
static void drawListItem(const DRAWITEMSTRUCT FAR *it){
 char text[360];char *f[4];RECT rc;COLORREF bg,fg;HBRUSH b;int sel,kind,n,ty;char *p;
 if((int)it->itemID<0)return;
 rc=it->rcItem;sel=(it->itemState&ODS_SELECTED)!=0;
 bg=GetSysColor(sel?COLOR_HIGHLIGHT:COLOR_WINDOW);fg=GetSysColor(sel?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT);
 b=CreateSolidBrush(bg);FillRect(it->hDC,&rc,b);DeleteObject(b);
 text[0]=0;SendMessage(it->hwndItem,LB_GETTEXT,it->itemID,(LPARAM)(LPSTR)text);
 kind=(int)SendMessage(it->hwndItem,LB_GETITEMDATA,it->itemID,0);
 f[0]=text;f[1]=f[2]=f[3]="";
 for(p=text,n=1;*p&&n<4;p++)if(*p=='\t'){*p=0;f[n++]=p+1;}
 drawFileGlyph(it->hDC,rc.left+2,rc.top,kind);
 ty=rc.top+2;
 SetBkMode(it->hDC,TRANSPARENT);SetTextColor(it->hDC,fg);
 TextOut(it->hDC,rc.left+Z7_COL_NAME,ty,(LPSTR)f[0],lstrlen(f[0]));
 textRight(it->hDC,rc.left+Z7_COL_SIZE_R,ty,f[1]);
 TextOut(it->hDC,rc.left+Z7_COL_DATE,ty,(LPSTR)f[2],lstrlen(f[2]));
 TextOut(it->hDC,rc.left+Z7_COL_ATTR,ty,(LPSTR)f[3],lstrlen(f[3]));
 if(it->itemState&ODS_FOCUS)DrawFocusRect(it->hDC,&rc);
}
/* The column-header bar painted above the list (no Win16 Header control). */
LRESULT FAR PASCAL __export Z7HeaderProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
 if(m==WM_PAINT){
  PAINTSTRUCT ps;RECT rc;HDC dc;HPEN hi,sh,op;
  dc=BeginPaint(w,&ps);GetClientRect(w,&rc);
  FillRect(dc,&rc,(HBRUSH)(COLOR_BTNFACE+1));
  hi=CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNHIGHLIGHT));sh=CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNSHADOW));
  op=(HPEN)SelectObject(dc,hi);MoveTo(dc,0,0);LineTo(dc,rc.right,0);
  SelectObject(dc,sh);MoveTo(dc,0,rc.bottom-1);LineTo(dc,rc.right,rc.bottom-1);
  MoveTo(dc,Z7_COL_SIZE_L-8,2);LineTo(dc,Z7_COL_SIZE_L-8,rc.bottom-2);
  MoveTo(dc,Z7_COL_DATE-8,2);LineTo(dc,Z7_COL_DATE-8,rc.bottom-2);
  MoveTo(dc,Z7_COL_ATTR-8,2);LineTo(dc,Z7_COL_ATTR-8,rc.bottom-2);
  SelectObject(dc,op);DeleteObject(hi);DeleteObject(sh);
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,GetSysColor(COLOR_BTNTEXT));
  TextOut(dc,Z7_COL_NAME,3,"Name",4);
  TextOut(dc,Z7_COL_SIZE_L,3,"Size",4);
  TextOut(dc,Z7_COL_DATE,3,"Modified",8);
  TextOut(dc,Z7_COL_ATTR,3,"Attr",4);
  EndPaint(w,&ps);return 0;
 }
 if(m==WM_ERASEBKGND)return 1;
 return DefWindowProc(w,m,wp,lp);
}
#ifdef __WATCOMC__
#pragma code_seg()
#endif
#endif
