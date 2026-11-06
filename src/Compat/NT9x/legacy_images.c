/* Image list storage for the NT 3.1 controls implemented by this port.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
static HMODULE common_module(void){HMODULE module=GetModuleHandleA("comctl32.dll");return module?module:LoadLibraryA("comctl32.dll");}
#define BIND(name, bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes) = legacy_##name
#define NATIVE(name) ((__typeof__(&name))GetProcAddress(common_module(),#name))
typedef struct Images {struct Images *next;int width,height,count,capacity,grow;UINT flags;HICON *icons;} Images;
static Images *lists;
static Images *find(HIMAGELIST h){Images *p;for(p=lists;p;p=p->next)if((HIMAGELIST)p==h)return p;SetLastError(ERROR_INVALID_HANDLE);return NULL;}
static BOOL reserve(Images *p,int size){HICON *icons;int cap,i;if(size<=p->capacity)return TRUE;if(size<0||size>0x1000000){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
 cap=size+p->grow;if(cap<size)cap=size;icons=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,cap*sizeof(*icons));if(!icons)return FALSE;
 for(i=0;i<p->count;i++)icons[i]=p->icons[i];if(p->icons)HeapFree(GetProcessHeap(),0,p->icons);p->icons=icons;p->capacity=cap;return TRUE;}
static HIMAGELIST WINAPI legacy_ImageList_Create(int width,int height,UINT flags,int initial,int grow){
 __typeof__(&ImageList_Create) fn=NATIVE(ImageList_Create);Images *p;if(fn)return fn(width,height,flags,initial,grow);
 if(width<=0||height<=0||initial<0||grow<0){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
 p=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*p));if(!p)return NULL;p->width=width;p->height=height;p->flags=flags;p->grow=grow;
 if(!reserve(p,initial)){HeapFree(GetProcessHeap(),0,p);return NULL;}p->next=lists;lists=p;return (HIMAGELIST)p;
}
BIND(ImageList_Create,20);
static BOOL WINAPI legacy_ImageList_Destroy(HIMAGELIST list){__typeof__(&ImageList_Destroy) fn=NATIVE(ImageList_Destroy);Images **link=&lists,*p;int i;if(fn)return fn(list);
 while((p=*link)!=NULL){if((HIMAGELIST)p==list){*link=p->next;for(i=0;i<p->count;i++)DestroyIcon(p->icons[i]);if(p->icons)HeapFree(GetProcessHeap(),0,p->icons);HeapFree(GetProcessHeap(),0,p);return TRUE;}link=&p->next;}SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
BIND(ImageList_Destroy,4);
static int WINAPI legacy_ImageList_GetImageCount(HIMAGELIST list){__typeof__(&ImageList_GetImageCount) fn=NATIVE(ImageList_GetImageCount);Images *p;if(fn)return fn(list);p=find(list);return p?p->count:0;}
BIND(ImageList_GetImageCount,4);
/* NT 3.1 CreateIconIndirect normalizes even a 16x16 bitmap to the system
   32x32 icon size. CreateIcon retains explicit dimensions. */
static HICON create_sized_icon(const ICONINFO *info,int width,int height){
 BITMAP mask,color;BYTE *maskBits=NULL,*colorBits=NULL;DWORD maskSize,colorSize=0;HICON result=NULL;
 if(!GetObjectA(info->hbmMask,sizeof(mask),&mask))return NULL;
 if(mask.bmHeight<=0||mask.bmWidthBytes<=0||(DWORD)mask.bmHeight>0x2000000UL/(DWORD)mask.bmWidthBytes)return NULL;
 maskSize=mask.bmHeight*mask.bmWidthBytes;maskBits=HeapAlloc(GetProcessHeap(),0,maskSize);if(!maskBits)return NULL;
 if(GetBitmapBits(info->hbmMask,maskSize,maskBits)!=(LONG)maskSize)goto done;
 if(info->hbmColor){
  if(!GetObjectA(info->hbmColor,sizeof(color),&color)||color.bmHeight<=0||color.bmWidthBytes<=0||(DWORD)color.bmHeight>0x2000000UL/(DWORD)color.bmWidthBytes)goto done;
  colorSize=color.bmHeight*color.bmWidthBytes;
  if(!color.bmPlanes||(DWORD)color.bmPlanes>0x2000000UL/colorSize)goto done;
  colorSize*=color.bmPlanes;
  colorBits=HeapAlloc(GetProcessHeap(),0,colorSize);if(!colorBits)goto done;
  if(GetBitmapBits(info->hbmColor,colorSize,colorBits)!=(LONG)colorSize)goto done;
  result=CreateIcon(GetModuleHandleA(NULL),width,height,(BYTE)color.bmPlanes,(BYTE)color.bmBitsPixel,maskBits,colorBits);
 }else result=CreateIcon(GetModuleHandleA(NULL),width,height,1,1,maskBits,maskBits+maskSize/2);
 done:if(colorBits)HeapFree(GetProcessHeap(),0,colorBits);HeapFree(GetProcessHeap(),0,maskBits);return result;
}
static BOOL resizeMask(HDC dst,int width,int height,HDC src,int sw,int sh){
 DWORD version=GetVersion();int x,y;
 if((version&0x800000ffUL)!=0x80000003UL)
  return StretchBlt(dst,0,0,width,height,src,0,0,sw,sh,SRCCOPY);
 /* Match COLORONCOLOR's pixel-center sampling for the monochrome mask;
    Win3's monochrome StretchBlt uses different edge sampling. */
 for(y=0;y<height;y++)for(x=0;x<width;x++){
  COLORREF pixel=GetPixel(src,(int)(((DWORD)x*2+1)*sw/((DWORD)width*2)),(int)(((DWORD)y*2+1)*sh/((DWORD)height*2)));
  if(pixel==CLR_INVALID||SetPixel(dst,x,y,pixel)==CLR_INVALID)return FALSE;
 }
 return TRUE;
}
HICON LegacySizedIcon(HICON source,int width,int height){
 ICONINFO in,out;BITMAP bm;HDC screen,src,dst;HGDIOBJ oldsrc,olddst;HICON result=NULL;int source_height;
 if(!GetIconInfo(source,&in))return NULL;
 if(!GetObjectA(in.hbmMask,sizeof(bm),&bm))goto release;
 source_height=in.hbmColor?bm.bmHeight:bm.bmHeight/2;
 /* Preserve the native color-plane layout when no resize is needed.
    Reconstructing VGA planar bitmap bits can change the artwork on Win32s. */
 if(bm.bmWidth==width&&source_height==height){result=CopyIcon(source);goto release;}
 ZeroMemory(&out,sizeof(out));out.fIcon=TRUE;
 screen=GetDC(NULL);src=CreateCompatibleDC(screen);dst=CreateCompatibleDC(screen);
 out.hbmMask=CreateBitmap(width,in.hbmColor?height:height*2,1,1,NULL);
 if(in.hbmColor)out.hbmColor=CreateCompatibleBitmap(screen,width,height);
 if(src&&dst&&out.hbmMask&&(!in.hbmColor||out.hbmColor)) {
  oldsrc=SelectObject(src,in.hbmMask);olddst=SelectObject(dst,out.hbmMask);SetStretchBltMode(dst,COLORONCOLOR);
  if(resizeMask(dst,width,in.hbmColor?height:height*2,src,bm.bmWidth,bm.bmHeight)) {
   BOOL ok=TRUE;
   if(in.hbmColor){SelectObject(src,in.hbmColor);SelectObject(dst,out.hbmColor);ok=StretchBlt(dst,0,0,width,height,src,0,0,bm.bmWidth,source_height,SRCCOPY);}
   SelectObject(src,oldsrc);SelectObject(dst,olddst);
   if(ok)result=create_sized_icon(&out,width,height);
  }else{SelectObject(src,oldsrc);SelectObject(dst,olddst);}
 }
 if(src)DeleteDC(src);if(dst)DeleteDC(dst);if(out.hbmMask)DeleteObject(out.hbmMask);if(out.hbmColor)DeleteObject(out.hbmColor);if(screen)ReleaseDC(NULL,screen);
release:
 DeleteObject(in.hbmMask);if(in.hbmColor)DeleteObject(in.hbmColor);return result;
}
static int WINAPI legacy_ImageList_ReplaceIcon(HIMAGELIST list,int index,HICON icon){__typeof__(&ImageList_ReplaceIcon) fn=NATIVE(ImageList_ReplaceIcon);Images *p;HICON copy;
 if(fn){
  __typeof__(&ImageList_GetIconSize) sizeFn=NATIVE(ImageList_GetIconSize);int width,height,result;
  /* Older common controls crop oversized icons instead of scaling them. */
  if(!sizeFn||!sizeFn(list,&width,&height))return -1;
  {
   ICONINFO info;BITMAP bitmap;BOOL same=FALSE;
   typedef HANDLE (WINAPI *CopyImageFn)(HANDLE,UINT,int,int,UINT);
   CopyImageFn resize=(CopyImageFn)GetProcAddress(GetModuleHandleA("USER32.DLL"),"CopyImage");
   if((GetVersion()&0x800000ffUL)==0x80000003UL)resize=NULL;
   if(GetIconInfo(icon,&info)){
    if(GetObjectA(info.hbmMask,sizeof(bitmap),&bitmap))same=bitmap.bmWidth==width&&bitmap.bmHeight==(info.hbmColor?height:height*2);
    DeleteObject(info.hbmMask);if(info.hbmColor)DeleteObject(info.hbmColor);
   }
   if(same)return fn(list,index,icon);
   /* Native CopyImage retains the OS-specific color bitmap representation. */
   copy=resize?(HICON)resize(icon,IMAGE_ICON,width,height,0):LegacySizedIcon(icon,width,height);
   if(!copy)return -1;
  }
  result=fn(list,index,copy);DestroyIcon(copy);return result;
 }
 p=find(list);
 if(!p)return -1;if(index < -1||index>=p->count){SetLastError(ERROR_INVALID_PARAMETER);return -1;}if(!reserve(p,p->count+(index==-1)))return -1;
 copy=LegacySizedIcon(icon,p->width,p->height);if(!copy)return -1;if(index==-1)index=p->count++;else DestroyIcon(p->icons[index]);p->icons[index]=copy;return index;}
BIND(ImageList_ReplaceIcon,12);
static int WINAPI legacy_ImageList_AddMasked(HIMAGELIST list,HBITMAP bitmap,COLORREF transparent){
 __typeof__(&ImageList_AddMasked) fn=NATIVE(ImageList_AddMasked);Images *p;BITMAP bm;HDC screen,src,color,mask;HGDIOBJ oldsrc,oldcolor,oldmask;HBITMAP colorbm,maskbm;int first,n,i;BOOL ok=TRUE;COLORREF oldbk;
 if(fn)return fn(list,bitmap,transparent);p=find(list);if(!p)return -1;
 if(!GetObjectA(bitmap,sizeof(bm),&bm)||bm.bmWidth<p->width||bm.bmHeight<p->height){SetLastError(ERROR_INVALID_PARAMETER);return -1;}
 n=bm.bmWidth/p->width;first=p->count;if(!reserve(p,first+n))return -1;
 screen=GetDC(NULL);if(!screen)return -1;src=CreateCompatibleDC(screen);color=CreateCompatibleDC(screen);mask=CreateCompatibleDC(screen);
 colorbm=CreateCompatibleBitmap(screen,p->width,p->height);maskbm=CreateBitmap(p->width,p->height,1,1,NULL);
 if(!src||!color||!mask||!colorbm||!maskbm){ok=FALSE;goto cleanup;}
 oldsrc=SelectObject(src,bitmap);oldcolor=SelectObject(color,colorbm);oldmask=SelectObject(mask,maskbm);
 if(transparent==CLR_DEFAULT)transparent=GetPixel(src,0,0);oldbk=SetBkColor(src,transparent);
 for(i=0;i<n;i++){
  ICONINFO info;HICON icon;
  if(!BitBlt(color,0,0,p->width,p->height,src,i*p->width,0,SRCCOPY)||!BitBlt(mask,0,0,p->width,p->height,src,i*p->width,0,SRCCOPY)){ok=FALSE;break;}
  /* Convert the transparent region of the color plane to black. */
  SetTextColor(color,RGB(0,0,0));SetBkColor(color,RGB(255,255,255));
  if(!BitBlt(color,0,0,p->width,p->height,mask,0,0,0x00220326UL)){ok=FALSE;break;} /* SRCAND NOT mask */
  ZeroMemory(&info,sizeof(info));info.fIcon=TRUE;info.hbmMask=maskbm;info.hbmColor=colorbm;
  SelectObject(color,oldcolor);SelectObject(mask,oldmask);
  icon=create_sized_icon(&info,p->width,p->height);
  SelectObject(color,colorbm);SelectObject(mask,maskbm);
  if(!icon){ok=FALSE;break;}p->icons[p->count++]=icon;
 }
 SetBkColor(src,oldbk);SelectObject(src,oldsrc);SelectObject(color,oldcolor);SelectObject(mask,oldmask);
cleanup:
 if(colorbm)DeleteObject(colorbm);if(maskbm)DeleteObject(maskbm);if(src)DeleteDC(src);if(color)DeleteDC(color);if(mask)DeleteDC(mask);ReleaseDC(NULL,screen);
 if(!ok){while(p->count>first)DestroyIcon(p->icons[--p->count]);return -1;}return first;
}
BIND(ImageList_AddMasked,12);
/* Draw at the stored dimensions without DrawIconEx (absent on NT 3.1).
   The custom list view calls this helper; native lists remain native. */
BOOL LegacyImageListDraw(HIMAGELIST list,int index,HDC target,int x,int y){
 __typeof__(&ImageList_Draw) fn=NATIVE(ImageList_Draw);Images *p;ICONINFO info;HDC source;HGDIOBJ old;COLORREF oldText,oldBackground;BOOL ok;
 if(fn)return fn(list,index,target,x,y,ILD_TRANSPARENT);p=find(list);if(!p||index<0||index>=p->count)return FALSE;
 if(!GetIconInfo(p->icons[index],&info))return FALSE;source=CreateCompatibleDC(target);
 if(!source){DeleteObject(info.hbmMask);if(info.hbmColor)DeleteObject(info.hbmColor);return FALSE;}
 /* Monochrome AND/XOR planes require black/white expansion even when
    the caller is painting selected text with different DC colors. */
 oldText=SetTextColor(target,RGB(0,0,0));oldBackground=SetBkColor(target,RGB(255,255,255));
 old=SelectObject(source,info.hbmMask);ok=BitBlt(target,x,y,p->width,p->height,source,0,0,SRCAND);
 if(info.hbmColor)SelectObject(source,info.hbmColor);
 ok=BitBlt(target,x,y,p->width,p->height,source,0,info.hbmColor?0:p->height,SRCINVERT)&&ok;
 SetTextColor(target,oldText);SetBkColor(target,oldBackground);
 SelectObject(source,old);DeleteDC(source);DeleteObject(info.hbmMask);if(info.hbmColor)DeleteObject(info.hbmColor);return ok;
}
