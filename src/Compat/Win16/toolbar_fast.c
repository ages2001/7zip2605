/* Cached transparent toolbar drawing for protected Windows modes.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include "toolbar.h"
void Z7Win16_FreeToolbarCache(Z7ToolbarCache FAR *cache){
 unsigned i;
 for(i=0;i<7;i++){
  if(cache->color[i])DeleteObject(cache->color[i]);
  if(cache->mask[i])DeleteObject(cache->mask[i]);
 }
 memset(cache,0,sizeof(*cache));
}
BOOL Z7Win16_InitToolbarCache(Z7ToolbarCache FAR *cache,const BYTE FAR *pixels){
 struct { BITMAPINFOHEADER header; RGBQUAD palette[16]; } info;
 BYTE mask[96];unsigned i,row,col,key,index;HDC dc;
 memset(cache,0,sizeof(*cache));dc=GetDC(0);if(!dc)return FALSE;
 memset(&info,0,sizeof(info));info.header.biSize=sizeof(BITMAPINFOHEADER);
 info.header.biWidth=24;info.header.biHeight=24;info.header.biPlanes=1;
 info.header.biBitCount=4;info.header.biCompression=BI_RGB;
 for(i=0;i<7;i++){
  const BYTE FAR *data=pixels+i*352;
  memcpy(info.palette,data,64);key=data[64+276]>>4;
  memset(&info.palette[key],0,sizeof(RGBQUAD));
  cache->color[i]=CreateDIBitmap(dc,&info.header,CBM_INIT,data+64,(BITMAPINFO FAR *)&info,DIB_RGB_COLORS);
  /* CreateBitmap takes top-down word-aligned monochrome rows. */
  memset(mask,0,sizeof(mask));
  for(row=0;row<24;row++)for(col=0;col<24;col++){
   index=data[64+(23-row)*12+col/2];index=(col&1)?index&15:index>>4;
   if(index==key)mask[row*4+col/8]|=0x80>>(col&7);
  }
  cache->mask[i]=CreateBitmap(24,24,1,1,mask);
  if(!cache->color[i]||!cache->mask[i]){ReleaseDC(0,dc);Z7Win16_FreeToolbarCache(cache);return FALSE;}
 }
 ReleaseDC(0,dc);cache->ready=TRUE;return TRUE;
}
BOOL Z7Win16_DrawToolbarCache(const Z7ToolbarCache FAR *cache,unsigned index,HDC dc,int x,int y){
 HDC source;HGDIOBJ old;BOOL ok;DWORD foreground,background;
 if(!cache->ready||index>=7)return FALSE;
 source=CreateCompatibleDC(dc);if(!source)return FALSE;
 foreground=SetTextColor(dc,RGB(0,0,0));background=SetBkColor(dc,RGB(255,255,255));
 old=SelectObject(source,cache->mask[index]);ok=BitBlt(dc,x,y,24,24,source,0,0,SRCAND);
 SelectObject(source,cache->color[index]);ok=BitBlt(dc,x,y,24,24,source,0,0,SRCPAINT)&&ok;
 SelectObject(source,old);DeleteDC(source);SetTextColor(dc,foreground);SetBkColor(dc,background);return ok;
}
