/* Toolbar control for NT 3.1, using only its native USER/GDI primitives.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
extern BOOL LegacyImageListDraw(HIMAGELIST,int,HDC,int,int);
typedef struct Button {TBBUTTON value;char *text;RECT rect;} Button;
typedef struct Toolbar {Button *buttons;int count,bw,bh,iw,ih,rows,pressed,focus;HFONT font;HIMAGELIST images,owned;char **strings;int nstrings;UINT structsize;HWND tip;int hover;BOOL tipVisible;} Toolbar;
static char *duplicate(LPCSTR s){char *p;if(!s)return NULL;p=HeapAlloc(GetProcessHeap(),0,lstrlenA(s)+1);if(p)lstrcpyA(p,s);return p;}
static char *narrow(LPCWSTR s){int n;char *p;if(!s)return NULL;n=WideCharToMultiByte(CP_ACP,0,s,-1,NULL,0,NULL,NULL);if(!n)return NULL;p=HeapAlloc(GetProcessHeap(),0,n);if(p)WideCharToMultiByte(CP_ACP,0,s,-1,p,n,NULL,NULL);return p;}
static void release(void *p){if(p)HeapFree(GetProcessHeap(),0,p);}
/* Original GDI artwork for the stock toolbar strips absent from NT 3.1.
   The public index order matches the common-controls STD/VIEW/HIST constants. */
static void stroke(HDC dc,int x,int y,int a,int b){MoveToEx(dc,x,y,NULL);LineTo(dc,a,b);}
static void folderGlyph(HDC dc,int x,int y){HBRUSH b=CreateSolidBrush(RGB(255,215,64));HGDIOBJ old=SelectObject(dc,b);Rectangle(dc,x+1,y+5,x+15,y+14);Rectangle(dc,x+2,y+3,x+8,y+6);SelectObject(dc,old);DeleteObject(b);}
static HBITMAP stockBitmap(UINT_PTR id,int *count,int *size){
 HDC screen,dc;HBITMAP bitmap;HGDIOBJ oldBitmap,oldPen,oldBrush,oldFont;HPEN pen;HFONT font;int family,i;RECT r;HBRUSH background;
 switch(id){
 case IDB_STD_SMALL_COLOR:case IDB_STD_LARGE_COLOR:family=0;*count=15;break;
 case IDB_VIEW_SMALL_COLOR:case IDB_VIEW_LARGE_COLOR:family=1;*count=12;break;
 case IDB_HIST_SMALL_COLOR:case IDB_HIST_LARGE_COLOR:family=2;*count=5;break;
 default:SetLastError(ERROR_RESOURCE_NAME_NOT_FOUND);return NULL;
 }
 *size=(id==IDB_STD_LARGE_COLOR||id==IDB_VIEW_LARGE_COLOR||id==IDB_HIST_LARGE_COLOR)?24:16;
 screen=GetDC(NULL);if(!screen)return NULL;dc=CreateCompatibleDC(screen);bitmap=CreateCompatibleBitmap(screen,*count*16,16);ReleaseDC(NULL,screen);
 if(!dc||!bitmap){if(dc)DeleteDC(dc);if(bitmap)DeleteObject(bitmap);return NULL;}
 oldBitmap=SelectObject(dc,bitmap);pen=CreatePen(PS_SOLID,1,RGB(0,0,0));oldPen=SelectObject(dc,pen);oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
 font=CreateFontA(11,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");oldFont=SelectObject(dc,font?font:GetStockObject(SYSTEM_FONT));
 SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(0,0,128));SetRect(&r,0,0,*count*16,16);background=CreateSolidBrush(RGB(255,0,255));FillRect(dc,&r,background);DeleteObject(background);
 for(i=0;i<*count;i++){
  int x=i*16,j;SetRect(&r,x+1,1,x+15,15);
  if(family==1){
   if(i<4){for(j=0;j<(i==0?4:6);j++){int a=x+2+(i==0?(j%2)*7:0),b=2+(i==0?(j/2)*7:j*2);Rectangle(dc,a,b,a+(i==0?5:2),b+(i==0?5:2));if(i>0)stroke(dc,a+4,b,a+11,b);}}
   else if(i<8){const char *labels[]={"A","9","D","T"};DrawTextA(dc,labels[i-4],1,&r,DT_LEFT|DT_SINGLELINE);stroke(dc,x+12,2,x+12,12);stroke(dc,x+9,9,x+12,12);stroke(dc,x+12,12,x+15,9);}
   else if(i==8||i==11){folderGlyph(dc,x,0);if(i==8){stroke(dc,x+8,12,x+8,4);stroke(dc,x+4,8,x+8,4);stroke(dc,x+8,4,x+12,8);}else{stroke(dc,x+11,0,x+11,6);stroke(dc,x+8,3,x+14,3);}}
   else{Rectangle(dc,x+1,2,x+10,9);stroke(dc,x+5,9,x+5,12);stroke(dc,x+2,12,x+14,12);stroke(dc,x+12,7,x+12,14);if(i==10){stroke(dc,x+1,1,x+14,14);stroke(dc,x+1,14,x+14,1);}}
  }else if(family==2){
   if(i<2){int direction=i?1:-1,center=x+8;stroke(dc,center-direction*5,8,center+direction*5,8);stroke(dc,center+direction*5,8,center,3);stroke(dc,center+direction*5,8,center,13);}
   else if(i==2)folderGlyph(dc,x,0);
   else if(i==3){const char star[]="*";DrawTextA(dc,star,1,&r,DT_CENTER|DT_SINGLELINE);}
   else{Rectangle(dc,x+2,2,x+14,14);stroke(dc,x+5,5,x+11,5);stroke(dc,x+5,8,x+11,8);}
  }else{
   switch(i){
   case STD_CUT:Ellipse(dc,x+1,9,x+6,15);Ellipse(dc,x+9,9,x+14,15);stroke(dc,x+3,2,x+11,12);stroke(dc,x+12,2,x+4,12);break;
   case STD_COPY:Rectangle(dc,x+2,2,x+10,12);Rectangle(dc,x+6,5,x+14,15);break;
   case STD_PASTE:Rectangle(dc,x+2,3,x+13,15);Rectangle(dc,x+5,1,x+10,5);break;
   case STD_UNDO:case STD_REDOW:{int a=i==STD_UNDO?3:13,b=i==STD_UNDO?13:3,c=i==STD_UNDO?7:9;stroke(dc,x+a,5,x+b,5);stroke(dc,x+b,5,x+b,12);stroke(dc,x+a,5,x+c,1);stroke(dc,x+a,5,x+c,9);break;}
   case STD_DELETE:stroke(dc,x+3,3,x+13,13);stroke(dc,x+3,13,x+13,3);break;
   case STD_FILENEW:Rectangle(dc,x+3,1,x+13,15);break;
   case STD_FILEOPEN:folderGlyph(dc,x,0);break;
   case STD_FILESAVE:Rectangle(dc,x+2,1,x+14,15);Rectangle(dc,x+5,2,x+11,6);Rectangle(dc,x+5,9,x+11,14);break;
   case STD_PRINTPRE:Rectangle(dc,x+2,1,x+11,13);Ellipse(dc,x+7,6,x+13,12);stroke(dc,x+12,11,x+15,15);break;
   case STD_PRINT:Rectangle(dc,x+1,5,x+15,12);Rectangle(dc,x+4,1,x+12,6);Rectangle(dc,x+4,10,x+12,15);break;
   case STD_FIND:Ellipse(dc,x+1,3,x+11,13);stroke(dc,x+10,12,x+15,15);break;
   case STD_REPLACE:DrawTextA(dc,"AB",2,&r,DT_CENTER|DT_SINGLELINE);stroke(dc,x+2,13,x+14,13);break;
   case STD_HELP:DrawTextA(dc,"?",1,&r,DT_CENTER|DT_SINGLELINE);break;
   case STD_PROPERTIES:Rectangle(dc,x+2,1,x+14,15);for(j=4;j<13;j+=3)stroke(dc,x+5,j,x+12,j);break;
   }
  }
 }
 SelectObject(dc,oldFont);if(font)DeleteObject(font);SelectObject(dc,oldPen);DeleteObject(pen);SelectObject(dc,oldBrush);SelectObject(dc,oldBitmap);
 if(*size==24){HDC scaled=CreateCompatibleDC(dc);HBITMAP larger;HGDIOBJ previous;screen=GetDC(NULL);larger=CreateCompatibleBitmap(screen,*count*24,24);ReleaseDC(NULL,screen);if(!scaled||!larger){if(scaled)DeleteDC(scaled);if(larger)DeleteObject(larger);DeleteObject(bitmap);DeleteDC(dc);return NULL;}previous=SelectObject(scaled,larger);SelectObject(dc,bitmap);StretchBlt(scaled,0,0,*count*24,24,dc,0,0,*count*16,16,SRCCOPY);SelectObject(scaled,previous);SelectObject(dc,oldBitmap);DeleteDC(scaled);DeleteObject(bitmap);bitmap=larger;}
 DeleteDC(dc);return bitmap;
}

static int command(Toolbar *t,int id){int i;for(i=0;i<t->count;i++)if(t->buttons[i].value.idCommand==id)return i;return -1;}
static void layout(HWND w,Toolbar *t,BOOL resize){RECT client;int i,x=0,y=0,width=0;LONG style=GetWindowLongA(w,GWL_STYLE);GetClientRect(GetParent(w),&client);
 for(i=0;i<t->count;i++){Button *b=t->buttons+i;int bw=(b->value.fsStyle&TBSTYLE_SEP)?(b->value.iBitmap>0?b->value.iBitmap:8):t->bw;
  SetRectEmpty(&b->rect);if(b->value.fsState&TBSTATE_HIDDEN)continue;
  if(x&&x+bw>client.right&&(style&TBSTYLE_WRAPABLE)){x=0;y+=t->bh;}
  SetRect(&b->rect,x,y,x+bw,y+t->bh);x+=bw;if(x>width)width=x;
  if(b->value.fsState&TBSTATE_WRAP){x=0;y+=t->bh;}
 }
 t->rows=y/t->bh+1;
 if(resize&&!(style&CCS_NORESIZE)){int top=0;if((style&CCS_BOTTOM)==CCS_BOTTOM)top=client.bottom-t->rows*t->bh;SetWindowPos(w,NULL,0,top,client.right,t->rows*t->bh,SWP_NOZORDER|SWP_NOACTIVATE);}
 InvalidateRect(w,NULL,TRUE);
}
static int hit(Toolbar *t,LPARAM l){POINT p={(short)LOWORD(l),(short)HIWORD(l)};int i;for(i=0;i<t->count;i++)if(PtInRect(&t->buttons[i].rect,p)&&!(t->buttons[i].value.fsStyle&TBSTYLE_SEP))return i;return -1;}
static void edge(HDC dc,RECT r,BOOL down){HBRUSH light=CreateSolidBrush(GetSysColor(down?COLOR_BTNSHADOW:COLOR_BTNHIGHLIGHT));HBRUSH dark=CreateSolidBrush(GetSysColor(down?COLOR_BTNHIGHLIGHT:COLOR_BTNSHADOW));RECT a;
 a=r;a.bottom=a.top+1;FillRect(dc,&a,light);a=r;a.right=a.left+1;FillRect(dc,&a,light);a=r;a.top=a.bottom-1;FillRect(dc,&a,dark);a=r;a.left=a.right-1;FillRect(dc,&a,dark);DeleteObject(light);DeleteObject(dark);}
static void activate(HWND w,Toolbar *t,int i){Button *b;int j;if(i<0||i>=t->count)return;b=t->buttons+i;if(!(b->value.fsState&TBSTATE_ENABLED))return;
 if(b->value.fsStyle&TBSTYLE_CHECK){if(b->value.fsStyle&TBSTYLE_GROUP){for(j=i-1;j>=0&&(t->buttons[j].value.fsStyle&TBSTYLE_GROUP);j--)t->buttons[j].value.fsState&=~TBSTATE_CHECKED;for(j=i+1;j<t->count&&(t->buttons[j].value.fsStyle&TBSTYLE_GROUP);j++)t->buttons[j].value.fsState&=~TBSTATE_CHECKED;b->value.fsState|=TBSTATE_CHECKED;}else b->value.fsState^=TBSTATE_CHECKED;}
 InvalidateRect(w,NULL,TRUE);SendMessageA(GetParent(w),WM_COMMAND,MAKEWPARAM(b->value.idCommand,0),(LPARAM)w);
}
static LRESULT CALLBACK tipWindow(HWND w,UINT msg,WPARAM wp,LPARAM lp){if(msg==WM_NCHITTEST)return HTTRANSPARENT;if(msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;char text[512];HBRUSH brush=CreateSolidBrush(RGB(255,255,225));HGDIOBJ font=SelectObject(dc,GetStockObject(SYSTEM_FONT));GetClientRect(w,&r);FillRect(dc,&r,brush);DeleteObject(brush);GetWindowTextA(w,text,sizeof(text));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(0,0,0));r.left+=4;r.top+=2;DrawTextA(dc,text,-1,&r,DT_LEFT|DT_SINGLELINE);SelectObject(dc,font);EndPaint(w,&ps);return 0;}return DefWindowProcA(w,msg,wp,lp);}
static void hideTip(HWND w,Toolbar *t){KillTimer(w,0x7a31);if(t->tip)ShowWindow(t->tip,SW_HIDE);t->tipVisible=FALSE;t->hover=-1;}
static BOOL showTip(HWND w,Toolbar *t){NMTTDISPINFOA info;char text[512];WNDCLASSA wc;HDC dc;SIZE size;HGDIOBJ font;POINT point;if(t->hover<0||t->hover>=t->count)return FALSE;ZeroMemory(&info,sizeof(info));info.hdr.hwndFrom=w;info.hdr.idFrom=t->buttons[t->hover].value.idCommand;info.hdr.code=TTN_GETDISPINFOA;info.lpszText=info.szText;SendMessageA(GetParent(w),WM_NOTIFY,info.hdr.idFrom,(LPARAM)&info);if(!IsWindow(w)||(Toolbar*)GetWindowLongA(w,0)!=t)return FALSE;text[0]=0;if(info.hinst&&IS_INTRESOURCE(info.lpszText))LoadStringA(info.hinst,LOWORD(info.lpszText),text,sizeof(text));else if(info.lpszText){UINT n=0;while(n+1<sizeof(text)&&info.lpszText[n]){text[n]=info.lpszText[n];n++;}text[n]=0;}if(!text[0]&&t->buttons[t->hover].text){UINT n=0;while(n+1<sizeof(text)&&t->buttons[t->hover].text[n]){text[n]=t->buttons[t->hover].text[n];n++;}text[n]=0;}if(!text[0])return FALSE;
 if(!t->tip){ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=tipWindow;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="Z7.NT31.ToolbarTip";if(!RegisterClassA(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return FALSE;t->tip=CreateWindowExA(WS_EX_TOPMOST,wc.lpszClassName,"",WS_POPUP|WS_BORDER,0,0,0,0,w,NULL,wc.hInstance,NULL);if(!t->tip)return FALSE;}
 dc=GetDC(w);font=SelectObject(dc,GetStockObject(SYSTEM_FONT));GetTextExtentPointA(dc,text,lstrlenA(text),&size);SelectObject(dc,font);ReleaseDC(w,dc);GetCursorPos(&point);point.y+=20;if(point.x+size.cx+10>GetSystemMetrics(SM_CXSCREEN))point.x=GetSystemMetrics(SM_CXSCREEN)-size.cx-10;if(point.y+size.cy+8>GetSystemMetrics(SM_CYSCREEN))point.y-=size.cy+30;SetWindowTextA(t->tip,text);SetWindowPos(t->tip,HWND_TOPMOST,point.x,point.y,size.cx+10,size.cy+8,SWP_NOACTIVATE|SWP_SHOWWINDOW);t->tipVisible=TRUE;return TRUE;
}
static LRESULT CALLBACK toolbar_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){Toolbar *t=(Toolbar*)GetWindowLongA(w,0);int i;LRESULT result;
 if(msg==WM_CREATE){t=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*t));if(!t)return -1;t->bw=24;t->bh=24;t->iw=t->ih=16;t->rows=1;t->pressed=t->focus=t->hover=-1;t->structsize=sizeof(TBBUTTON);t->font=GetStockObject(SYSTEM_FONT);SetWindowLongA(w,0,(LONG)t);return 0;}
 if(!t)return DefWindowProcA(w,msg,wp,lp);
 switch(msg){
 case WM_DESTROY:hideTip(w,t);if(t->tip)DestroyWindow(t->tip);for(i=0;i<t->count;i++)release(t->buttons[i].text);release(t->buttons);for(i=0;i<t->nstrings;i++)release(t->strings[i]);release(t->strings);if(t->owned)ImageList_Destroy(t->owned);release(t);SetWindowLongA(w,0,0);return 0;
 case WM_SETFONT:t->font=(HFONT)wp;if(lp)InvalidateRect(w,NULL,TRUE);return 0;
 case WM_GETFONT:return (LRESULT)t->font;
 case WM_WINDOWPOSCHANGING:{WINDOWPOS *pos=(WINDOWPOS*)lp;if(!(pos->flags&SWP_NOSIZE)&&!(GetWindowLongA(w,GWL_STYLE)&CCS_NORESIZE)){layout(w,t,FALSE);pos->cy=t->rows*t->bh;}break;}
 case WM_SIZE:layout(w,t,FALSE);return 0;
 case WM_ERASEBKGND:{RECT r;HBRUSH b=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));GetClientRect(w,&r);FillRect((HDC)wp,&r,b);DeleteObject(b);return TRUE;}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);HGDIOBJ oldfont=SelectObject(dc,t->font);SetBkMode(dc,TRANSPARENT);
  for(i=0;i<t->count;i++){Button *b=t->buttons+i;RECT r=b->rect;BOOL down=(i==t->pressed)||(b->value.fsState&(TBSTATE_CHECKED|TBSTATE_PRESSED));int offset=down?1:0;
   if(IsRectEmpty(&r)||b->value.fsStyle&TBSTYLE_SEP)continue;edge(dc,r,down);
   if(t->images&&b->value.iBitmap>=0)LegacyImageListDraw(t->images,b->value.iBitmap,dc,r.left+(r.right-r.left-t->iw)/2+offset,r.top+3+offset);
   if(b->text){r.top+=t->images?t->ih+4:3;r.left+=2+offset;r.right-=2;r.bottom-=2;SetTextColor(dc,GetSysColor((b->value.fsState&TBSTATE_ENABLED)?COLOR_BTNTEXT:COLOR_GRAYTEXT));DrawTextA(dc,b->text,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER);}
   if(i==t->focus&&GetFocus()==w){r=b->rect;InflateRect(&r,-3,-3);DrawFocusRect(dc,&r);}
  }SelectObject(dc,oldfont);EndPaint(w,&ps);return 0;}
 case WM_LBUTTONDOWN:hideTip(w,t);i=hit(t,lp);if(i>=0&&(t->buttons[i].value.fsState&TBSTATE_ENABLED)){t->pressed=i;t->focus=i;SetCapture(w);InvalidateRect(w,NULL,TRUE);}return 0;
 case WM_LBUTTONUP:i=hit(t,lp);{int previous=t->pressed;t->pressed=-1;ReleaseCapture();InvalidateRect(w,NULL,TRUE);if(i==previous)activate(w,t,i);}return 0;
 case WM_CANCELMODE:t->pressed=-1;ReleaseCapture();InvalidateRect(w,NULL,TRUE);return 0;
 case WM_MOUSEMOVE:if(GetWindowLongA(w,GWL_STYLE)&TBSTYLE_TOOLTIPS){i=hit(t,lp);if(i!=t->hover){hideTip(w,t);t->hover=i;if(i>=0)SetTimer(w,0x7a31,500,NULL);}}return 0;
 case WM_TIMER:if(wp==0x7a31){POINT p;int under;GetCursorPos(&p);if(WindowFromPoint(p)!=w){hideTip(w,t);return 0;}ScreenToClient(w,&p);under=hit(t,MAKELPARAM(p.x,p.y));if(under!=t->hover){hideTip(w,t);return 0;}if(!t->tipVisible)showTip(w,t);return 0;}break;
 case TB_GETTOOLTIPS:return (LRESULT)t->tip;
 case WM_KILLFOCUS:case WM_SETFOCUS:InvalidateRect(w,NULL,TRUE);return 0;
 case WM_GETDLGCODE:return DLGC_WANTARROWS;
 case WM_KEYDOWN:if(wp==VK_RETURN||wp==VK_SPACE){activate(w,t,t->focus);return 0;}if(wp==VK_LEFT||wp==VK_RIGHT){int step=wp==VK_LEFT?-1:1,n=t->count;for(i=t->focus+step;n-->0;i+=step){if(i<0)i=t->count-1;if(i>=t->count)i=0;if(!(t->buttons[i].value.fsStyle&TBSTYLE_SEP)&&!(t->buttons[i].value.fsState&TBSTATE_HIDDEN)&&(t->buttons[i].value.fsState&TBSTATE_ENABLED)){t->focus=i;break;}}InvalidateRect(w,NULL,TRUE);return 0;}break;
 case TB_BUTTONSTRUCTSIZE:if(wp!=sizeof(TBBUTTON)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}t->structsize=wp;return 0;
 case TB_SETBITMAPSIZE:if(!LOWORD(lp)||!HIWORD(lp))return FALSE;t->iw=LOWORD(lp);t->ih=HIWORD(lp);if(t->bw<t->iw+8)t->bw=t->iw+8;if(t->bh<t->ih+8)t->bh=t->ih+8;layout(w,t,FALSE);return TRUE;
 case TB_SETBUTTONSIZE:if(!LOWORD(lp)||!HIWORD(lp))return FALSE;t->bw=LOWORD(lp);t->bh=HIWORD(lp);layout(w,t,FALSE);return TRUE;
 case TB_GETBUTTONSIZE:return MAKELONG(t->bw,t->bh);
 case TB_SETIMAGELIST:{HIMAGELIST old=t->images;t->images=(HIMAGELIST)lp;InvalidateRect(w,NULL,TRUE);return (LRESULT)old;}
 case TB_GETIMAGELIST:return (LRESULT)t->images;
 case TB_ADDBITMAP:{TBADDBITMAP *add=(TBADDBITMAP*)lp;HBITMAP bitmap;BOOL owned;
  if(!add||wp>0x1000000)return -1;
  if(add->hInst==HINST_COMMCTRL){int count,size;bitmap=stockBitmap(add->nID,&count,&size);if(!bitmap)return -1;if(t->owned&&(t->iw!=size||t->ih!=size)){DeleteObject(bitmap);SetLastError(ERROR_INVALID_PARAMETER);return -1;}t->iw=t->ih=size;if(t->bw<size+8)t->bw=size+8;if(t->bh<size+8)t->bh=size+8;wp=count;owned=TRUE;}
  else {owned=add->hInst!=NULL;bitmap=owned?LoadBitmapA(add->hInst,MAKEINTRESOURCEA(add->nID)):(HBITMAP)add->nID;}if(!bitmap)return -1;
  if(!t->owned)t->owned=ImageList_Create(t->iw,t->ih,ILC_COLOR|ILC_MASK,wp,4);
  result=t->owned?ImageList_AddMasked(t->owned,bitmap,add->hInst==HINST_COMMCTRL?RGB(255,0,255):CLR_DEFAULT):-1;if(owned)DeleteObject(bitmap);if(result>=0)t->images=t->owned;return result;}
 case TB_ADDSTRINGA:case TB_ADDSTRINGW:{const void *s=(const void*)lp;char resource[4096];int first=t->nstrings;
  if(wp){if(!LoadStringA((HINSTANCE)wp,(UINT)lp,resource,sizeof(resource)))return -1;s=resource;}
  if(!s)return -1;
  for(;;){char *text;char **strings;
   if(wp||msg==TB_ADDSTRINGA){if(!*(const char*)s)break;text=duplicate(s);s=(const char*)s+lstrlenA(s)+1;}else{if(!*(const WCHAR*)s)break;text=narrow(s);s=(const WCHAR*)s+lstrlenW(s)+1;}
   if(!text)return -1;strings=HeapAlloc(GetProcessHeap(),0,(t->nstrings+1)*sizeof(*strings));if(!strings){release(text);return -1;}
   for(i=0;i<t->nstrings;i++)strings[i]=t->strings[i];strings[t->nstrings++]=text;release(t->strings);t->strings=strings;if(wp)break;
  }return first;}
 case TB_ADDBUTTONSA:case TB_ADDBUTTONSW:case TB_INSERTBUTTONA:case TB_INSERTBUTTONW:{BOOL insert=msg==TB_INSERTBUTTONA||msg==TB_INSERTBUTTONW;int n=insert?1:(int)wp,pos=insert?(int)wp:t->count,j;Button *buttons;const TBBUTTON *input=(const TBBUTTON*)lp;
  if(n<0||n>65536||pos<0||pos>t->count||!input)return FALSE;buttons=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(t->count+n)*sizeof(*buttons));if(!buttons)return FALSE;
  for(i=0;i<pos;i++)buttons[i]=t->buttons[i];for(i=pos;i<t->count;i++)buttons[i+n]=t->buttons[i];
  for(j=0;j<n;j++){Button *b=buttons+pos+j;INT_PTR str=input[j].iString;b->value=input[j];
   if(str>=0&&str<t->nstrings)b->text=duplicate(t->strings[str]);else if((UINT_PTR)str>65535&&str!=-1)b->text=(msg==TB_ADDBUTTONSW||msg==TB_INSERTBUTTONW)?narrow((LPCWSTR)str):duplicate((LPCSTR)str);
   if(((str>=0&&str<t->nstrings)||((UINT_PTR)str>65535&&str!=-1))&&!b->text){for(i=0;i<j;i++)release(buttons[pos+i].text);release(buttons);return FALSE;}
   if(b->text){HDC dc=GetDC(w);if(dc){SIZE size;HGDIOBJ old=SelectObject(dc,t->font);if(GetTextExtentPointA(dc,b->text,lstrlenA(b->text),&size)){if(t->bw<size.cx+8)t->bw=size.cx+8;if(t->bh<t->ih+size.cy+8)t->bh=t->ih+size.cy+8;}SelectObject(dc,old);ReleaseDC(w,dc);}}
  }release(t->buttons);t->buttons=buttons;t->count+=n;layout(w,t,FALSE);return TRUE;}
 case TB_DELETEBUTTON:if(wp>=(UINT)t->count)return FALSE;release(t->buttons[wp].text);for(i=wp+1;i<t->count;i++)t->buttons[i-1]=t->buttons[i];t->count--;layout(w,t,FALSE);return TRUE;
 case TB_GETBUTTON:if(wp>=(UINT)t->count||!lp)return FALSE;*(TBBUTTON*)lp=t->buttons[wp].value;return TRUE;
 case TB_BUTTONCOUNT:return t->count;
 case TB_COMMANDTOINDEX:return command(t,wp);
 case TB_GETSTATE:i=command(t,wp);return i<0?-1:t->buttons[i].value.fsState;
 case TB_SETSTATE:i=command(t,wp);if(i<0)return FALSE;t->buttons[i].value.fsState=(BYTE)LOWORD(lp);layout(w,t,FALSE);return TRUE;
 case TB_ENABLEBUTTON:case TB_CHECKBUTTON:case TB_PRESSBUTTON:case TB_HIDEBUTTON:{UINT bit=msg==TB_ENABLEBUTTON?TBSTATE_ENABLED:msg==TB_CHECKBUTTON?TBSTATE_CHECKED:msg==TB_PRESSBUTTON?TBSTATE_PRESSED:TBSTATE_HIDDEN;i=command(t,wp);if(i<0)return FALSE;if(LOWORD(lp))t->buttons[i].value.fsState|=bit;else t->buttons[i].value.fsState&=~bit;layout(w,t,FALSE);return TRUE;}
 case TB_ISBUTTONENABLED:case TB_ISBUTTONCHECKED:case TB_ISBUTTONPRESSED:case TB_ISBUTTONHIDDEN:{UINT bit=msg==TB_ISBUTTONENABLED?TBSTATE_ENABLED:msg==TB_ISBUTTONCHECKED?TBSTATE_CHECKED:msg==TB_ISBUTTONPRESSED?TBSTATE_PRESSED:TBSTATE_HIDDEN;i=command(t,wp);return i>=0&&(t->buttons[i].value.fsState&bit)!=0;}
 case TB_AUTOSIZE:layout(w,t,TRUE);return 0;
 case TB_GETROWS:return t->rows;
 case TB_GETITEMRECT:case TB_GETRECT:i=msg==TB_GETITEMRECT?(int)wp:command(t,wp);if(i<0||i>=t->count||!lp)return FALSE;*(RECT*)lp=t->buttons[i].rect;return TRUE;
 case TB_GETMAXSIZE:if(!lp)return FALSE;{SIZE *s=(SIZE*)lp;s->cx=s->cy=0;for(i=0;i<t->count;i++){if(t->buttons[i].rect.right>s->cx)s->cx=t->buttons[i].rect.right;if(t->buttons[i].rect.bottom>s->cy)s->cy=t->buttons[i].rect.bottom;}return TRUE;}
 }
 return DefWindowProcA(w,msg,wp,lp);
}
BOOL LegacyRegisterToolbar(void){WNDCLASSA wc;HMODULE module=GetModuleHandleA(NULL);if(GetClassInfoA(NULL,TOOLBARCLASSNAMEA,&wc)||GetClassInfoA(module,TOOLBARCLASSNAMEA,&wc))return TRUE;
 ZeroMemory(&wc,sizeof(wc));wc.style=CS_DBLCLKS;wc.lpfnWndProc=toolbar_proc;wc.cbWndExtra=sizeof(void*);wc.hInstance=module;wc.hCursor=LoadCursorA(NULL,IDC_ARROW);wc.lpszClassName=TOOLBARCLASSNAMEA;wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);return RegisterClassA(&wc)!=0;}
