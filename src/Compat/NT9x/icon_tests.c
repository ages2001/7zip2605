/* Verify extracted icon ownership, dimensions and resource identifiers.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
static HANDLE report;static unsigned failures;
static void check(BOOL ok,const char *text){DWORD n;WriteFile(report,ok?"PASS ":"FAIL ",5,&n,NULL);WriteFile(report,text,lstrlenA(text),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);if(!ok)failures++;}
static BOOL dimensions(HICON icon,int width,int height){ICONINFO info;BITMAP bitmap;BOOL ok;if(!icon||!GetIconInfo(icon,&info))return FALSE;ok=GetObjectA(info.hbmMask,sizeof(bitmap),&bitmap)&&bitmap.bmWidth==width&&bitmap.bmHeight==(info.hbmColor?height:2*height);if(!ok){char detail[128];DWORD n;wsprintfA(detail,"Icon mask: %ld x %ld; color: %u; expected: %d x %d\r\n",bitmap.bmWidth,bitmap.bmHeight,info.hbmColor!=NULL,width,height);WriteFile(report,detail,lstrlenA(detail),&n,NULL);}DeleteObject(info.hbmMask);if(info.hbmColor)DeleteObject(info.hbmColor);return ok;}
extern BOOL LegacyImageListDraw(HIMAGELIST,int,HDC,int,int);
static void pixels(void){
 BYTE mask[128],bits[128];HICON icon;HIMAGELIST list;HDC screen,dc,source;HBITMAP bitmap,small;HGDIOBJ old,oldSmall;int x,y,index;COLORREF background=RGB(0,128,128);HBRUSH fill;RECT area;
 ZeroMemory(mask,sizeof(mask));FillMemory(bits,sizeof(bits),255);
 for(y=24;y<32;y++)for(x=24;x<32;x++)bits[y*4+x/8]&=(BYTE)~(0x80>>(x%8));
 icon=CreateIcon(GetModuleHandleA(NULL),32,32,1,1,mask,bits);
 list=ImageList_Create(16,16,ILC_MASK,1,1);index=ImageList_ReplaceIcon(list,-1,icon);
 screen=GetDC(NULL);dc=CreateCompatibleDC(screen);bitmap=CreateCompatibleBitmap(screen,16,16);old=SelectObject(dc,bitmap);PatBlt(dc,0,0,16,16,WHITENESS);
 check(index==0&&LegacyImageListDraw(list,index,dc,0,0),"render full 32-pixel source into 16-pixel image list");
 check(GetPixel(dc,14,14)==RGB(0,0,0)&&GetPixel(dc,4,4)==RGB(255,255,255),"bottom-right source quadrant survives scaling instead of cropping");
 ImageList_Destroy(list);DestroyIcon(icon);
 source=CreateCompatibleDC(screen);small=CreateCompatibleBitmap(screen,16,16);oldSmall=SelectObject(source,small);PatBlt(source,0,0,16,16,WHITENESS);PatBlt(source,12,12,4,4,BLACKNESS);SelectObject(source,oldSmall);
 list=ImageList_Create(16,16,ILC_MASK,1,1);index=ImageList_AddMasked(list,small,RGB(255,255,255));
 SetRect(&area,0,0,16,16);fill=CreateSolidBrush(background);FillRect(dc,&area,fill);DeleteObject(fill);
 SetTextColor(dc,RGB(255,255,255));SetBkColor(dc,RGB(0,0,128));
 check(index==0&&LegacyImageListDraw(list,index,dc,0,0),"render small masked toolbar bitmap");
 check(GetPixel(dc,14,14)==RGB(0,0,0)&&GetPixel(dc,4,4)==background,"masked toolbar stays transparent on colored background with selection DC colors");
 check(GetTextColor(dc)==RGB(255,255,255)&&GetBkColor(dc)==RGB(0,0,128),"image drawing preserves caller selection colors");
 ImageList_Destroy(list);SelectObject(dc,old);DeleteObject(bitmap);DeleteObject(small);DeleteDC(source);DeleteDC(dc);ReleaseDC(NULL,screen);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show){char path[MAX_PATH];WCHAR wide[MAX_PATH];HICON big=NULL,small=NULL;UINT n;DWORD version=GetVersion();
 report=CreateFileA("C:\\ICONTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)return 2;
 GetModuleFileNameA(NULL,path,MAX_PATH);MultiByteToWideChar(CP_ACP,0,path,-1,wide,MAX_PATH);
 check(ExtractIconExW(wide,-1,NULL,NULL,0)==1,"count-only query finds embedded icon group");
 /* Native APIs can count both requested sizes; validate each owned handle. */
 n=ExtractIconExW(wide,0,&big,&small,1);check(n>=1&&big&&small&&big!=small,"extract independently owned large and small icons");check(dimensions(big,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON)),"large icon dimensions");check(dimensions(small,16,16),"small icon dimensions");if(big)DestroyIcon(big);if(small)DestroyIcon(small);
 if(!(version&0x80000000UL)||(version&255)==3){big=small=NULL;n=ExtractIconExW(wide,-101,&big,&small,1);check(n>=1,"negative resource extraction returns requested icons");check(dimensions(big,32,32)&&dimensions(small,16,16),"extract by negative resource identifier");if(big)DestroyIcon(big);if(small)DestroyIcon(small);}
 big=small=NULL;check(ExtractIconExW(wide,100,&big,&small,1)==0&&!big&&!small,"out-of-range ordinal creates no handles");
 if(!(version&0x80000000UL)&&(version&255)==3){
  HKEY extKey=NULL,classKey=NULL,iconKey=NULL;DWORD disposition;WCHAR ext[40],cls[80],value[2*MAX_PATH],sample[80];SHFILEINFOW info;int first;
  wsprintfW(ext,L".z7icon%08lX",GetTickCount());wsprintfW(cls,L"Z7IconFixture%08lX",GetTickCount());wsprintfW(sample,L"test%s",ext);
  check(RegCreateKeyExW(HKEY_CLASSES_ROOT,ext,0,NULL,0,KEY_ALL_ACCESS,NULL,&extKey,&disposition)==ERROR_SUCCESS&&disposition==REG_CREATED_NEW_KEY,"create isolated extension registration");
  check(RegCreateKeyExW(HKEY_CLASSES_ROOT,cls,0,NULL,0,KEY_ALL_ACCESS,NULL,&classKey,&disposition)==ERROR_SUCCESS&&disposition==REG_CREATED_NEW_KEY,"create isolated file class");
  if(extKey&&classKey){
   RegSetValueExW(extKey,NULL,0,REG_SZ,(BYTE*)cls,(lstrlenW(cls)+1)*2);
   RegSetValueExW(classKey,NULL,0,REG_SZ,(BYTE*)L"Icon fixture",26);
   check(RegCreateKeyExW(classKey,L"DefaultIcon",0,NULL,0,KEY_ALL_ACCESS,NULL,&iconKey,NULL)==ERROR_SUCCESS,"create icon association");
   if(iconKey){wsprintfW(value,L"\"%s\",-101",wide);RegSetValueExW(iconKey,NULL,0,REG_SZ,(BYTE*)value,(lstrlenW(value)+1)*2);RegCloseKey(iconKey);}
   ZeroMemory(&info,sizeof(info));check(SHGetFileInfoW(sample,FILE_ATTRIBUTE_NORMAL,&info,sizeof(info),SHGFI_USEFILEATTRIBUTES|SHGFI_TYPENAME|SHGFI_ICON|SHGFI_SYSICONINDEX|SHGFI_SMALLICON)!=0&&info.iIcon>=2&&!lstrcmpW(info.szTypeName,L"Icon fixture")&&dimensions(info.hIcon,16,16),"registered file type, icon and small dimensions");first=info.iIcon;if(info.hIcon)DestroyIcon(info.hIcon);
   ZeroMemory(&info,sizeof(info));check(SHGetFileInfoW(sample,FILE_ATTRIBUTE_NORMAL,&info,sizeof(info),SHGFI_USEFILEATTRIBUTES|SHGFI_SYSICONINDEX|SHGFI_SMALLICON)!=0&&info.iIcon==first,"association image index remains stable");
   SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
   ZeroMemory(&info,sizeof(info));check(SHGetFileInfoW(sample,FILE_ATTRIBUTE_NORMAL,&info,sizeof(info),SHGFI_USEFILEATTRIBUTES|SHGFI_ICON|SHGFI_SMALLICON)!=0&&dimensions(info.hIcon,16,16),"association change refreshes owned icon");if(info.hIcon)DestroyIcon(info.hIcon);
   RegDeleteKeyW(classKey,L"DefaultIcon");RegCloseKey(classKey);RegCloseKey(extKey);RegDeleteKeyW(HKEY_CLASSES_ROOT,cls);RegDeleteKeyW(HKEY_CLASSES_ROOT,ext);
  }
 }
 pixels();
 check(failures==0,"ALL");CloseHandle(report);return failures?1:0;
}
