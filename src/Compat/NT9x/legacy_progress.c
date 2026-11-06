/* NT 3.1 progress control, using native GDI and a full signed 32-bit range.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
typedef struct Progress{LONG low,high,position,step;COLORREF bar,background;UINT state,phase;BOOL marquee;} Progress;
static LONG clamp(Progress *p,LONGLONG n){return n<p->low?p->low:n>p->high?p->high:(LONG)n;}
static LRESULT CALLBACK progress(HWND w,UINT msg,WPARAM wp,LPARAM lp){Progress *p=(Progress*)GetWindowLongA(w,0);LONG old;if(msg==WM_CREATE){p=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*p));if(!p)return -1;p->high=100;p->step=10;p->bar=p->background=CLR_DEFAULT;p->state=PBST_NORMAL;SetWindowLongA(w,0,(LONG)p);return 0;}if(!p)return DefWindowProcA(w,msg,wp,lp);
 switch(msg){
 case WM_DESTROY:KillTimer(w,1);HeapFree(GetProcessHeap(),0,p);SetWindowLongA(w,0,0);return 0;
 case PBM_SETRANGE:case PBM_SETRANGE32:old=MAKELONG(p->low,p->high);{LONG low=msg==PBM_SETRANGE?(SHORT)LOWORD(lp):(LONG)wp,high=msg==PBM_SETRANGE?(SHORT)HIWORD(lp):(LONG)lp;if(high<low){SetLastError(ERROR_INVALID_PARAMETER);return old;}p->low=low;p->high=high;p->position=clamp(p,p->position);}InvalidateRect(w,NULL,FALSE);return old;
 case PBM_GETRANGE:if(lp){((PBRANGE*)lp)->iLow=p->low;((PBRANGE*)lp)->iHigh=p->high;}return wp?p->low:p->high;
 case PBM_SETPOS:old=p->position;p->position=clamp(p,(LONG)wp);InvalidateRect(w,NULL,FALSE);return old;
 case PBM_DELTAPOS:old=p->position;p->position=clamp(p,(LONGLONG)p->position+(LONG)wp);InvalidateRect(w,NULL,FALSE);return old;
 case PBM_GETPOS:return p->position;
 case PBM_SETSTEP:old=p->step;p->step=(LONG)wp;return old;
 case PBM_GETSTEP:return p->step;
 case PBM_STEPIT:old=p->position;{LONGLONG span=(LONGLONG)p->high-p->low,next=(LONGLONG)p->position+p->step;if(span>0){if(next>p->high)next=p->low+(next-p->low)%span;else if(next<p->low)next=p->high-(p->low-next)%span;}p->position=clamp(p,next);}InvalidateRect(w,NULL,FALSE);return old;
 case PBM_SETBARCOLOR:old=p->bar;p->bar=(COLORREF)lp;InvalidateRect(w,NULL,FALSE);return old;
 case PBM_SETBKCOLOR:old=p->background;p->background=(COLORREF)lp;InvalidateRect(w,NULL,FALSE);return old;
 case PBM_GETBARCOLOR:return p->bar;
 case PBM_GETBKCOLOR:return p->background;
 case PBM_SETSTATE:old=p->state;if(wp>=PBST_NORMAL&&wp<=PBST_PAUSED){p->state=wp;InvalidateRect(w,NULL,FALSE);}return old;
 case PBM_GETSTATE:return p->state;
 case PBM_SETMARQUEE:p->marquee=wp!=0;if(wp){if(!SetTimer(w,1,lp>0?(UINT)lp:30,NULL)){p->marquee=FALSE;return FALSE;}}else KillTimer(w,1);InvalidateRect(w,NULL,FALSE);return TRUE;
 case WM_TIMER:if(wp==1&&p->marquee){p->phase=(p->phase+4)%120;InvalidateRect(w,NULL,FALSE);return 0;}break;
 case WM_ERASEBKGND:return TRUE;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r,fill;HBRUSH back,bar;LONG style=GetWindowLongA(w,GWL_STYLE);int length,amount,i;LONGLONG span=(LONGLONG)p->high-p->low;COLORREF color=p->bar==CLR_DEFAULT?(p->state==PBST_ERROR?RGB(190,0,0):p->state==PBST_PAUSED?RGB(210,160,0):GetSysColor(COLOR_HIGHLIGHT)):p->bar;GetClientRect(w,&r);back=CreateSolidBrush(p->background==CLR_DEFAULT?GetSysColor(COLOR_BTNFACE):p->background);bar=CreateSolidBrush(color);FillRect(dc,&r,back);length=(style&PBS_VERTICAL)?r.bottom:r.right;amount=span>0?(int)(((LONGLONG)p->position-p->low)*length/span):0;
  if(p->marquee){int start=(int)p->phase*length/100-length/5,end=start+length/5;fill=r;if(style&PBS_VERTICAL){fill.top=r.bottom-end;fill.bottom=r.bottom-start;}else{fill.left=start;fill.right=end;}IntersectRect(&fill,&fill,&r);FillRect(dc,&fill,bar);}
  else if(style&PBS_SMOOTH){fill=r;if(style&PBS_VERTICAL)fill.top=fill.bottom-amount;else fill.right=amount;FillRect(dc,&fill,bar);}
  else for(i=0;i<amount;i+=8){int end=i+6;if(end>amount)end=amount;fill=r;if(style&PBS_VERTICAL){fill.top=r.bottom-end;fill.bottom=r.bottom-i;}else{fill.left=i;fill.right=end;}FillRect(dc,&fill,bar);}
  DeleteObject(back);DeleteObject(bar);EndPaint(w,&ps);return 0;}
 }
 return DefWindowProcA(w,msg,wp,lp);
}
BOOL LegacyRegisterProgress(void){WNDCLASSA wc;HINSTANCE instance=GetModuleHandleA(NULL);/* A local class takes precedence over the system common-control class.
   Old native classes exist but do not implement the required 32-bit messages. */
if(GetClassInfoA(instance,PROGRESS_CLASSA,&wc)&&wc.lpfnWndProc==progress)return TRUE;
{DWORD version=GetVersion();if(!(version&0x80000000UL)&&(version&255)>=5&&GetClassInfoA(NULL,PROGRESS_CLASSA,&wc))return TRUE;}ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=progress;wc.cbWndExtra=sizeof(void*);wc.hInstance=instance;wc.lpszClassName=PROGRESS_CLASSA;return RegisterClassA(&wc)!=0;}
