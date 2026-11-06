/* Original indexed artwork with transparent owner-drawn button backgrounds.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include "toolbar.h"
#define TOOLBAR_ICON_BYTES 352
static HGLOBAL toolbarStorage;
static BYTE FAR *toolbarPixels;
static Z7ToolbarCache toolbarCache;
void Z7Win16_FreeToolbar(void){
 if(toolbarCache.ready)Z7Win16_FreeToolbarCache(&toolbarCache);
 if(toolbarStorage){if(toolbarPixels)GlobalUnlock(toolbarStorage);GlobalFree(toolbarStorage);}
 toolbarStorage=0;toolbarPixels=NULL;
}
BOOL Z7Win16_InitToolbar(HINSTANCE instance){
 unsigned i;HGLOBAL loaded=0;const BYTE FAR *data=NULL;
 toolbarStorage=GlobalAlloc(GMEM_MOVEABLE,7UL*TOOLBAR_ICON_BYTES);if(!toolbarStorage)return FALSE;
 toolbarPixels=(BYTE FAR *)GlobalLock(toolbarStorage);if(!toolbarPixels)goto failed;
 for(i=0;i<7;i++){
  HRSRC resource;DWORD length,offset;BYTE FAR *out=toolbarPixels+i*TOOLBAR_ICON_BYTES;
  resource=FindResource(instance,MAKEINTRESOURCE(2001+i),RT_RCDATA);if(!resource)goto failed;
  length=SizeofResource(instance,resource);loaded=LoadResource(instance,resource);if(!loaded)goto failed;
  data=(const BYTE FAR *)LockResource(loaded);if(!data)goto failed;
  if(length<118||data[0]!='B'||data[1]!='M'||*(const DWORD FAR *)(data+14)!=40||
     *(const DWORD FAR *)(data+18)!=24||*(const DWORD FAR *)(data+22)!=24||
     *(const WORD FAR *)(data+26)!=1||*(const WORD FAR *)(data+28)!=4||
     *(const DWORD FAR *)(data+30)!=BI_RGB)goto failed;
  offset=*(const DWORD FAR *)(data+10);if(offset<118||offset>length||length-offset<288)goto failed;
  memcpy(out,data+54,64);memcpy(out+64,data+offset,288);
  UnlockResource(loaded);FreeResource(loaded);loaded=0;data=NULL;
 }
 if(GetWinFlags()&(WF_STANDARD|WF_ENHANCED))Z7Win16_InitToolbarCache(&toolbarCache,toolbarPixels);
 return TRUE;
 failed:if(loaded){if(data)UnlockResource(loaded);FreeResource(loaded);}Z7Win16_FreeToolbar();return FALSE;
}
static BOOL drawArtwork(HINSTANCE instance,UINT id,HDC target,int x,int y){
 const BYTE FAR *data;unsigned row,col,index,key;BOOL ok=TRUE;(void)instance;
 if(id<1001||id>1007||!toolbarPixels)return FALSE;data=toolbarPixels+(id-1001)*TOOLBAR_ICON_BYTES;key=data[64+276]>>4;
 if(toolbarCache.ready)return Z7Win16_DrawToolbarCache(&toolbarCache,id-1001,target,x,y);
 for(row=0;row<24;row++)for(col=0;col<24;col++){
  const BYTE FAR *rgb;index=data[64+(23-row)*12+col/2];index=(col&1)?index&15:index>>4;
  if(index==key)continue;rgb=data+index*4;
  if(SetPixel(target,x+col,y+row,RGB(rgb[2],rgb[1],rgb[0]))==(DWORD)-1)ok=FALSE;
 }
 return ok;
}
static BOOL toolbarFill(HDC dc,const RECT FAR *rect,DWORD color){
 SetBkColor(dc,color);return ExtTextOut(dc,0,0,ETO_OPAQUE,rect,NULL,0,NULL);
}
BOOL Z7Win16_DrawToolbar(HINSTANCE instance,const DRAWITEMSTRUCT FAR *item,UINT bitmapId){
 RECT rect,label,edge,focus;char text[32];BOOL ok;int saved,pressed;DWORD light,dark;
 if(!item||item->CtlType!=ODT_BUTTON)return FALSE;
 saved=SaveDC(item->hDC);if(!saved)return FALSE;
 rect=item->rcItem;pressed=!!(item->itemState&ODS_SELECTED);
 ok=toolbarFill(item->hDC,&rect,GetSysColor(COLOR_BTNFACE));
 light=pressed?GetSysColor(COLOR_BTNSHADOW):RGB(255,255,255);
 dark=pressed?RGB(255,255,255):GetSysColor(COLOR_BTNSHADOW);
 edge=rect;edge.bottom=edge.top+1;ok=toolbarFill(item->hDC,&edge,light)&&ok;
 edge=rect;edge.right=edge.left+1;ok=toolbarFill(item->hDC,&edge,light)&&ok;
 edge=rect;edge.top=edge.bottom-1;ok=toolbarFill(item->hDC,&edge,dark)&&ok;
 edge=rect;edge.left=edge.right-1;ok=toolbarFill(item->hDC,&edge,dark)&&ok;
 ok=drawArtwork(instance,bitmapId,item->hDC,rect.left+(rect.right-rect.left-24)/2+pressed,rect.top+4+pressed)&&ok;
 text[0]=0;if(!GetWindowText(item->hwndItem,text,sizeof(text)))ok=FALSE;
 label=rect;label.top+=30+pressed;label.bottom-=3;label.left+=2+pressed;label.right-=2;
 SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,GetSysColor(item->itemState&ODS_DISABLED?COLOR_BTNSHADOW:COLOR_BTNTEXT));
 DrawText(item->hDC,text,-1,&label,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
 if(item->itemState&ODS_FOCUS){focus=rect;InflateRect(&focus,-3,-3);DrawFocusRect(item->hDC,&focus);}
 if(!RestoreDC(item->hDC,saved))ok=FALSE;return ok;
}
