/* Filesystem identifiers and shell information on NT 3.1.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "legacy_shell.h"
void *Z7ShellAlloc(SIZE_T n){return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,n);}
void Z7ShellFree(void *p){if(p)HeapFree(GetProcessHeap(),0,p);}
FARPROC Z7ShellNative(const char *name){DWORD version=GetVersion();HMODULE m;if((version&255)==3)return NULL;/* Keep the complete NT 3.x / Win32s PIDL family together; partial Shell32 exports and Win32s stubs must not receive our private identifiers. */m=GetModuleHandleA("shell32.dll");if(!m)m=LoadLibraryA("shell32.dll");return m?GetProcAddress(m,name):NULL;}
BOOL Z7ShellEqual(REFGUID a,REFGUID b){const DWORD *x=(const DWORD*)a,*y=(const DWORD*)b;return x[0]==y[0]&&x[1]==y[1]&&x[2]==y[2]&&x[3]==y[3];}
BOOL Z7ShellWide(const char *s,WCHAR *d,UINT size){if(!s||!d){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}return MultiByteToWideChar(CP_ACP,0,s,-1,d,size)!=0;}
BOOL Z7ShellAnsi(const WCHAR *s,char *d,UINT size){BOOL used=FALSE;int n;if(!s||!d){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}n=WideCharToMultiByte(CP_ACP,0,s,-1,d,size,NULL,&used);if(used){SetLastError(ERROR_NO_UNICODE_TRANSLATION);return FALSE;}return n!=0;}
const WCHAR *Z7ShellBase(const WCHAR *s){const WCHAR *p=s;while(*s){if(*s=='\\'||*s=='/')p=s+1;s++;}return *p?p:s;}
BOOL Z7ShellJoin(const WCHAR *parent,const WCHAR *name,WCHAR *out){WCHAR temp[Z7_PATH];DWORD n;UINT len;if(!name){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}if(name[0]=='\\'||(name[0]&&name[1]==':')){if(lstrlenW(name)>=Z7_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}lstrcpyW(temp,name);}else{len=parent?lstrlenW(parent):0;if(len+lstrlenW(name)+2>Z7_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}temp[0]=0;if(len){lstrcpyW(temp,parent);if(temp[len-1]!='\\')lstrcatW(temp,L"\\");}lstrcatW(temp,name);}n=GetFullPathNameW(temp,Z7_PATH,out,NULL);if(!n||n>=Z7_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}return TRUE;}
/* Small extension DLLs only need the shared path helpers above. */
#ifndef Z7_SHELL_PATHS_ONLY
/* Each item is self-contained; the trailing zero SHITEMID remains a real PIDL
   terminator. A signature prevents foreign PIDLs from becoming path pointers. */
LPITEMIDLIST Z7ShellPidl(UINT kind,const WCHAR *path){UINT len=path?lstrlenW(path):0,cb=8+(len+1)*2;BYTE *p;if(len>=Z7_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return NULL;}p=CoTaskMemAlloc(cb+2);if(!p)return NULL;ZeroMemory(p,cb+2);*(USHORT*)p=(USHORT)cb;*(USHORT*)(p+2)=0x375a;*(USHORT*)(p+4)=0x3153;*(USHORT*)(p+6)=(USHORT)kind;if(path)CopyMemory(p+8,path,(len+1)*2);return (LPITEMIDLIST)p;}
UINT Z7ShellDecode(LPCITEMIDLIST list,WCHAR *path){const BYTE *p=(const BYTE*)list;UINT kind=Z7_DESKTOP;if(path)path[0]=0;if(!p)return 0;while(*(const USHORT*)p){UINT cb=*(const USHORT*)p,len,i;if(cb<10||cb>8+Z7_PATH*2||cb%2||*(const USHORT*)(p+2)!=0x375a||*(const USHORT*)(p+4)!=0x3153)return 0;kind=*(const USHORT*)(p+6);if(kind<Z7_DESKTOP||kind>Z7_FILE)return 0;len=(cb-8)/2;for(i=0;i<len&&((const WCHAR*)(p+8))[i];i++);if(i==len)return 0;if(path)lstrcpyW(path,(const WCHAR*)(p+8));p+=cb;}return kind;}
BOOL WINAPI SHGetPathFromIDListW(LPCITEMIDLIST id,LPWSTR path){__typeof__(&SHGetPathFromIDListW) fn=SHELL_NATIVE(SHGetPathFromIDListW);UINT kind;if(fn)return fn(id,path);if(!path)return FALSE;kind=Z7ShellDecode(id,path);if(kind!=Z7_FILE){path[0]=0;SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}return TRUE;}
SHELL_BIND(SHGetPathFromIDListW,8);
BOOL WINAPI SHGetPathFromIDListA(LPCITEMIDLIST id,LPSTR path){__typeof__(&SHGetPathFromIDListA) fn=SHELL_NATIVE(SHGetPathFromIDListA);WCHAR w[Z7_PATH];if(fn)return fn(id,path);if(!path)return FALSE;path[0]=0;return SHGetPathFromIDListW(id,w)&&Z7ShellAnsi(w,path,Z7_PATH);}
SHELL_BIND(SHGetPathFromIDListA,8);
static BOOL folderPath(int csidl,WCHAR *path){const WCHAR *value=NULL,*suffix=NULL;HKEY key;DWORD type,size=Z7_PATH*2,n;WCHAR raw[Z7_PATH];BOOL common=FALSE;
 switch(csidl){case CSIDL_DESKTOPDIRECTORY:value=L"Desktop";suffix=L"Desktop";break;case CSIDL_PROGRAMS:value=L"Programs";suffix=L"Start Menu\\Programs";break;case CSIDL_STARTMENU:value=L"Start Menu";suffix=L"Start Menu";break;case CSIDL_STARTUP:value=L"Startup";suffix=L"Start Menu\\Programs\\Startup";break;case CSIDL_PERSONAL:value=L"Personal";suffix=L"Personal";break;case CSIDL_SENDTO:value=L"SendTo";suffix=L"SendTo";break;case CSIDL_FONTS:return GetWindowsDirectoryW(path,Z7_PATH)&&lstrlenW(path)+7<Z7_PATH?(lstrcatW(path,L"\\FONTS"),TRUE):FALSE;case CSIDL_COMMON_PROGRAMS:common=TRUE;value=L"Common Programs";suffix=L"Start Menu\\Programs";break;case CSIDL_COMMON_DESKTOPDIRECTORY:common=TRUE;value=L"Common Desktop";suffix=L"Desktop";break;default:SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 if(RegOpenKeyExW(common?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",0,KEY_READ,&key)==ERROR_SUCCESS){LONG e=RegQueryValueExW(key,value,NULL,&type,(BYTE*)raw,&size);RegCloseKey(key);if(!e&&(type==REG_SZ||type==REG_EXPAND_SZ)&&size>=2&&size<=sizeof(raw)&&raw[size/2-1]==0){n=ExpandEnvironmentStringsW(raw,path,Z7_PATH);if(n&&n<=Z7_PATH)return TRUE;}}
 n=GetEnvironmentVariableW(common?L"ALLUSERSPROFILE":L"USERPROFILE",raw,Z7_PATH);if(!n||n>=Z7_PATH){n=GetWindowsDirectoryW(raw,Z7_PATH);if(!n||n>=Z7_PATH)return FALSE;}return Z7ShellJoin(raw,suffix,path);
}
HRESULT WINAPI SHGetSpecialFolderLocation(HWND owner,int csidl,LPITEMIDLIST *out){__typeof__(&SHGetSpecialFolderLocation) fn=SHELL_NATIVE(SHGetSpecialFolderLocation);WCHAR path[Z7_PATH];UINT kind;(void)owner;if(fn)return fn(owner,csidl,out);if(!out)return E_POINTER;*out=NULL;csidl&=0xff;kind=csidl==CSIDL_DESKTOP?Z7_DESKTOP:csidl==CSIDL_DRIVES?Z7_COMPUTER:csidl==CSIDL_NETWORK?Z7_NETWORK:Z7_FILE;path[0]=0;if(kind==Z7_FILE&&!folderPath(csidl,path))return HRESULT_FROM_WIN32(GetLastError());*out=Z7ShellPidl(kind,path);return *out?S_OK:E_OUTOFMEMORY;}
SHELL_BIND(SHGetSpecialFolderLocation,12);
DWORD Z7ShellAttributes(UINT kind,const WCHAR *path){DWORD file,a;if(kind!=Z7_FILE)return SFGAO_FOLDER|SFGAO_HASSUBFOLDER|SFGAO_DROPTARGET;file=GetFileAttributesW(path);if(file==INVALID_FILE_ATTRIBUTES)return 0;a=SFGAO_FILESYSTEM|SFGAO_CANCOPY|SFGAO_CANMOVE|SFGAO_CANRENAME|SFGAO_CANDELETE|SFGAO_HASPROPSHEET;if(file&FILE_ATTRIBUTE_DIRECTORY)a|=SFGAO_FOLDER|SFGAO_FILESYSANCESTOR|SFGAO_HASSUBFOLDER|SFGAO_DROPTARGET;if(file&FILE_ATTRIBUTE_READONLY)a|=SFGAO_READONLY;if(file&FILE_ATTRIBUTE_HIDDEN)a|=SFGAO_HIDDEN;return a;}
static HIMAGELIST smallImages,largeImages;static LONG imageLock;static LONG iconGeneration;
/* Original VGA-friendly file, folder, computer, network, desktop and drive
   glyphs. Draw both mask and color planes so no desktop color is baked in. */
static void iconBox(HDC color,HDC mask,int x,int y,int w,int h,COLORREF rgb){
 RECT r;HBRUSH brush=CreateSolidBrush(rgb);SetRect(&r,x*2,y*2,(x+w)*2,(y+h)*2);
 FillRect(color,&r,brush);FillRect(mask,&r,GetStockObject(BLACK_BRUSH));DeleteObject(brush);
}
static HICON shellIcon(UINT kind){
 HDC screen=GetDC(NULL),color=NULL,mask=NULL;HBITMAP cb=NULL,mb=NULL;HGDIOBJ oc=NULL,om=NULL;ICONINFO info;HICON result=NULL;
 if(!screen)return NULL;color=CreateCompatibleDC(screen);mask=CreateCompatibleDC(screen);cb=CreateCompatibleBitmap(screen,32,32);mb=CreateBitmap(32,32,1,1,NULL);
 if(!color||!mask||!cb||!mb)goto done;oc=SelectObject(color,cb);om=SelectObject(mask,mb);PatBlt(color,0,0,32,32,BLACKNESS);PatBlt(mask,0,0,32,32,WHITENESS);
 #define BOX(x,y,w,h,c) iconBox(color,mask,x,y,w,h,c)
 if(kind==0){
  BOX(3,1,10,14,RGB(0,0,0));BOX(4,2,8,12,RGB(255,255,255));
  BOX(5,5,6,1,RGB(128,128,128));BOX(5,8,6,1,RGB(128,128,128));BOX(5,11,4,1,RGB(128,128,128));
 }else if(kind==1){
  BOX(1,3,7,3,RGB(0,0,0));BOX(2,4,5,2,RGB(255,255,0));
  BOX(1,5,14,9,RGB(0,0,0));BOX(2,6,12,7,RGB(255,255,0));BOX(2,12,12,1,RGB(128,128,0));
 }else if(kind==2||kind==4){
  BOX(2,1,12,10,RGB(0,0,0));BOX(3,2,10,8,RGB(192,192,192));BOX(4,3,8,6,RGB(0,0,128));BOX(5,4,6,1,RGB(0,255,255));
  BOX(6,11,4,2,RGB(0,0,0));BOX(2,13,12,2,RGB(0,0,0));BOX(3,13,10,1,RGB(192,192,192));
 }else if(kind==3){
  BOX(1,2,6,6,RGB(0,0,0));BOX(2,3,4,3,RGB(0,128,128));BOX(9,2,6,6,RGB(0,0,0));BOX(10,3,4,3,RGB(0,128,128));
  BOX(3,8,1,4,RGB(0,0,0));BOX(11,8,1,4,RGB(0,0,0));BOX(3,11,9,1,RGB(0,0,0));BOX(7,11,1,3,RGB(0,0,0));BOX(5,13,5,2,RGB(0,0,128));
 }else{
  BOX(1,5,14,8,RGB(0,0,0));BOX(2,6,12,6,RGB(192,192,192));BOX(3,7,10,1,RGB(255,255,255));BOX(3,10,7,1,RGB(0,0,0));BOX(12,10,1,1,RGB(0,128,0));
 }
 #undef BOX
 SelectObject(color,oc);SelectObject(mask,om);oc=om=NULL;ZeroMemory(&info,sizeof(info));info.fIcon=TRUE;info.hbmColor=cb;info.hbmMask=mb;result=CreateIconIndirect(&info);
 done:if(oc)SelectObject(color,oc);if(om)SelectObject(mask,om);if(cb)DeleteObject(cb);if(mb)DeleteObject(mb);if(color)DeleteDC(color);if(mask)DeleteDC(mask);ReleaseDC(NULL,screen);return result;
}
static BOOL images(void){BOOL ok=TRUE;unsigned i;while(InterlockedExchange(&imageLock,1))Sleep(0);if(!smallImages||!largeImages){smallImages=ImageList_Create(16,16,ILC_MASK,6,8);largeImages=ImageList_Create(32,32,ILC_MASK,6,8);ok=smallImages&&largeImages;for(i=0;i<6&&ok;i++){HICON icon=shellIcon(i);ok=icon&&ImageList_ReplaceIcon(smallImages,-1,icon)==(int)i&&ImageList_ReplaceIcon(largeImages,-1,icon)==(int)i;if(icon)DestroyIcon(icon);}if(!ok){if(smallImages)ImageList_Destroy(smallImages);if(largeImages)ImageList_Destroy(largeImages);smallImages=largeImages=NULL;}}InterlockedExchange(&imageLock,0);return ok;}
/* Association icons have stable image-list indices. On association changes
   discard the lookup cache but retain existing list entries used by panels. */
typedef struct AssocIcon {struct AssocIcon *next;WCHAR path[Z7_PATH];int ordinal,index;HICON large,small;} AssocIcon;
static AssocIcon *assocIcons;static UINT assocCount;
static BOOL classTextW(const WCHAR *key,WCHAR *out,UINT capacity){HKEY h;DWORD type,size=capacity*2;LONG e=RegOpenKeyExW(HKEY_CLASSES_ROOT,key,0,KEY_QUERY_VALUE,&h);if(e)return FALSE;e=RegQueryValueExW(h,NULL,NULL,&type,(BYTE*)out,&size);RegCloseKey(h);return !e&&(type==REG_SZ||type==REG_EXPAND_SZ)&&size>=2&&!(size&1)&&size<=capacity*2&&out[size/2-1]==0;}
/* Win32s implements no Unicode registry entry points (RegOpenKeyExW and
   RegQueryValueExW fail with ERROR_CALL_NOT_IMPLEMENTED), so the Unicode path
   above returns nothing there. Fall back to the ANSI API and widen the result
   so file associations resolve their type name and icon on Win32s too. NT and
   9x keep using the Unicode path, which already worked. */
static BOOL classTextA(const WCHAR *key,WCHAR *out,UINT capacity){
 HKEY h;DWORD type,size;LONG e;char keyA[Z7_PATH],valA[Z7_PATH];int n;
 if(!WideCharToMultiByte(CP_ACP,0,key,-1,keyA,sizeof(keyA),NULL,NULL))return FALSE;
 if(RegOpenKeyExA(HKEY_CLASSES_ROOT,keyA,0,KEY_QUERY_VALUE,&h))return FALSE;
 size=sizeof(valA)-1;e=RegQueryValueExA(h,NULL,NULL,&type,(BYTE*)valA,&size);RegCloseKey(h);
 if(e||(type!=REG_SZ&&type!=REG_EXPAND_SZ)||!size)return FALSE;
 if(valA[size-1]!=0){if(size>=sizeof(valA))return FALSE;valA[size]=0;}
 n=MultiByteToWideChar(CP_ACP,0,valA,-1,out,capacity);
 return n>0&&n<=(int)capacity;
}
static BOOL classText(const WCHAR *key,WCHAR *out,UINT capacity){
 return classTextW(key,out,capacity)||classTextA(key,out,capacity);
}
static BOOL association(const WCHAR *path,WCHAR *module,int *ordinal,WCHAR *type){
 const WCHAR *base=Z7ShellBase(path),*ext=NULL,*p;WCHAR cls[Z7_PATH],key[Z7_PATH],raw[Z7_PATH],expanded[Z7_PATH];UINT i,n;int comma=-1;BOOL quoted=FALSE;
 for(p=base;*p;p++)if(*p=='.')ext=p;
 if(!ext||!classText(ext,cls,Z7_PATH)||!cls[0])return FALSE;
 if(type){WCHAR friendly[80];if(classText(cls,friendly,80))lstrcpyW(type,friendly);}
 if(lstrlenW(cls)+13>=Z7_PATH)return FALSE;lstrcpyW(key,cls);lstrcatW(key,L"\\DefaultIcon");
 if(!classText(key,raw,Z7_PATH))return FALSE;
 n=ExpandEnvironmentStringsW(raw,expanded,Z7_PATH);
 if(!n||n>Z7_PATH){/* Win32s: no Unicode ExpandEnvironmentStringsW; use ANSI. */
  char rawA[Z7_PATH],expA[Z7_PATH];DWORD m;
  if(!WideCharToMultiByte(CP_ACP,0,raw,-1,rawA,sizeof(rawA),NULL,NULL))return FALSE;
  m=ExpandEnvironmentStringsA(rawA,expA,sizeof(expA));if(!m||m>sizeof(expA))return FALSE;
  if(MultiByteToWideChar(CP_ACP,0,expA,-1,expanded,Z7_PATH)<=0)return FALSE;}
 for(i=0;expanded[i];i++){if(expanded[i]=='"')quoted=!quoted;else if(expanded[i]==','&&!quoted)comma=(int)i;}
 *ordinal=0;
 if(comma>=0){int sign=1,value=0;WCHAR *number=expanded+comma+1;expanded[comma]=0;while(*number==' ')number++;if(*number=='-'){sign=-1;number++;}if(*number<'0'||*number>'9')return FALSE;while(*number>='0'&&*number<='9'){if(value>6553||(value==6553&&*number>'5'))return FALSE;value=value*10+*number++-'0';}while(*number==' ')number++;if(*number)return FALSE;*ordinal=sign*value;}
 p=expanded;while(*p==' ')p++;n=lstrlenW(p);while(n&&p[n-1]==' ')n--;
 if(n>=2&&p[0]=='"'&&p[n-1]=='"'){p++;n-=2;}
 if(n==2&&p[0]=='%'&&p[1]=='1'){lstrcpyW(module,path);return TRUE;}
 if(!n||n>=Z7_PATH)return FALSE;CopyMemory(module,p,n*2);module[n]=0;return TRUE;
}
extern HICON LegacySizedIcon(HICON,int,int);
/* Extract the association icon pair, falling back to the ANSI entry point on
   Win32s where ExtractIconExW is not implemented. */
static UINT extractAssocIcons(const WCHAR *module,int ordinal,HICON *large,HICON *small){
 UINT got=ExtractIconExW(module,ordinal,large,small,1);
 if(got==0){char modA[Z7_PATH];if(WideCharToMultiByte(CP_ACP,0,module,-1,modA,sizeof(modA),NULL,NULL))got=ExtractIconExA(modA,ordinal,large,small,1);}
 return got;
}
static BOOL associationIcon(const WCHAR *path,UINT flags,SHFILEINFOW *info){
 WCHAR module[Z7_PATH];int ordinal;AssocIcon *entry;BOOL result=FALSE;HICON large=NULL,small=NULL;
 if(!association(path,module,&ordinal,(flags&SHGFI_TYPENAME)?info->szTypeName:NULL))return FALSE;
 if(!(flags&(SHGFI_ICON|SHGFI_SYSICONINDEX)))return TRUE;
 if(!images())return FALSE;
 while(InterlockedExchange(&imageLock,1))Sleep(0);
 for(entry=assocIcons;entry;entry=entry->next)if(entry->ordinal==ordinal&&!lstrcmpiW(entry->path,module))break;
 if(!entry&&assocCount<1024&&extractAssocIcons(module,ordinal,&large,&small)!=0){
  entry=Z7ShellAlloc(sizeof(*entry));
  if(entry){int index=ImageList_ReplaceIcon(smallImages,-1,small),other=ImageList_ReplaceIcon(largeImages,-1,large);
   if(index>=0&&other==index){lstrcpyW(entry->path,module);entry->ordinal=ordinal;entry->index=index;entry->large=large;entry->small=small;large=small=NULL;entry->next=assocIcons;assocIcons=entry;assocCount++;}
   else{Z7ShellFree(entry);entry=NULL;}
  }
 }
 if(entry){info->iIcon=entry->index;if(flags&SHGFI_ICON)info->hIcon=LegacySizedIcon((flags&SHGFI_SMALLICON)?entry->small:entry->large,(flags&SHGFI_SMALLICON)?16:32,(flags&SHGFI_SMALLICON)?16:32);result=!(flags&SHGFI_ICON)||info->hIcon!=NULL;}
 if(large)DestroyIcon(large);if(small)DestroyIcon(small);InterlockedExchange(&imageLock,0);return result;
}
static void clearAssociations(void){AssocIcon *entry,*next;while(InterlockedExchange(&imageLock,1))Sleep(0);for(entry=assocIcons;entry;entry=next){next=entry->next;DestroyIcon(entry->large);DestroyIcon(entry->small);Z7ShellFree(entry);}assocIcons=NULL;assocCount=0;InterlockedExchange(&imageLock,0);}
DWORD_PTR WINAPI SHGetFileInfoW(LPCWSTR input,DWORD attrs,SHFILEINFOW *info,UINT size,UINT flags){__typeof__(&SHGetFileInfoW) fn=SHELL_NATIVE(SHGetFileInfoW);WCHAR path[Z7_PATH];UINT kind=Z7_FILE;DWORD mask;const WCHAR *name;BOOL dir;
 if(fn)return fn(input,attrs,info,size,flags);if(!info||size<sizeof(*info)||!input){SetLastError(ERROR_INVALID_PARAMETER);return 0;}mask=info->dwAttributes;ZeroMemory(info,sizeof(*info));if(flags&SHGFI_PIDL){kind=Z7ShellDecode((LPCITEMIDLIST)input,path);if(!kind){SetLastError(ERROR_INVALID_PARAMETER);return 0;}}else{if(lstrlenW(input)>=Z7_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return 0;}lstrcpyW(path,input);}if(!(flags&SHGFI_USEFILEATTRIBUTES)&&kind==Z7_FILE){attrs=GetFileAttributesW(path);if(attrs==INVALID_FILE_ATTRIBUTES)return 0;}dir=kind!=Z7_FILE||(attrs&FILE_ATTRIBUTE_DIRECTORY);info->iIcon=kind==Z7_COMPUTER?2:kind==Z7_NETWORK?3:kind==Z7_DESKTOP?4:dir?1:0;
 if(kind==Z7_FILE&&dir&&lstrlenW(path)==3&&path[1]==':'&&path[2]=='\\')info->iIcon=5;
 name=kind==Z7_DESKTOP?L"Desktop":kind==Z7_COMPUTER?L"My Computer":kind==Z7_NETWORK?L"Network":Z7ShellBase(path);
 if(flags&SHGFI_DISPLAYNAME)lstrcpyW(info->szDisplayName,name);if(flags&SHGFI_TYPENAME)lstrcpyW(info->szTypeName,dir?L"File Folder":L"File");if(flags&SHGFI_ATTRIBUTES){info->dwAttributes=Z7ShellAttributes(kind,path);if(flags&SHGFI_ATTR_SPECIFIED)info->dwAttributes&=mask;}
 if(flags&SHGFI_EXETYPE){SetLastError(ERROR_NOT_SUPPORTED);return 0;}
 if(!dir)associationIcon(path,flags,info);
 if((flags&SHGFI_ICON)&&!info->hIcon){info->hIcon=shellIcon(info->iIcon);if(info->hIcon&&(flags&SHGFI_SMALLICON)){HICON small=LegacySizedIcon(info->hIcon,16,16);DestroyIcon(info->hIcon);info->hIcon=small;}if(!info->hIcon)return 0;}
 if(flags&SHGFI_SYSICONINDEX){if(!images()){if(info->hIcon)DestroyIcon(info->hIcon);info->hIcon=NULL;return 0;}return (DWORD_PTR)((flags&SHGFI_SMALLICON)?smallImages:largeImages);}return 1;
}
SHELL_BIND(SHGetFileInfoW,20);
DWORD_PTR WINAPI SHGetFileInfoA(LPCSTR input,DWORD attrs,SHFILEINFOA *info,UINT size,UINT flags){__typeof__(&SHGetFileInfoA) fn=SHELL_NATIVE(SHGetFileInfoA);WCHAR path[Z7_PATH];SHFILEINFOW wide;DWORD_PTR result;if(fn)return fn(input,attrs,info,size,flags);if(!info||size<sizeof(*info)){SetLastError(ERROR_INVALID_PARAMETER);return 0;}ZeroMemory(&wide,sizeof(wide));wide.dwAttributes=info->dwAttributes;if(!(flags&SHGFI_PIDL)&&!Z7ShellWide(input,path,Z7_PATH))return 0;result=SHGetFileInfoW((flags&SHGFI_PIDL)?(LPCWSTR)input:path,attrs,&wide,sizeof(wide),flags);if(!result)return 0;ZeroMemory(info,sizeof(*info));info->hIcon=wide.hIcon;info->iIcon=wide.iIcon;info->dwAttributes=wide.dwAttributes;if(!Z7ShellAnsi(wide.szDisplayName,info->szDisplayName,MAX_PATH)||!Z7ShellAnsi(wide.szTypeName,info->szTypeName,80)){if(info->hIcon)DestroyIcon(info->hIcon);info->hIcon=NULL;return 0;}return result;}
SHELL_BIND(SHGetFileInfoA,20);
void WINAPI SHChangeNotify(LONG event,UINT flags,LPCVOID first,LPCVOID second){__typeof__(&SHChangeNotify) fn=SHELL_NATIVE(SHChangeNotify);if(fn){fn(event,flags,first,second);return;}/* No external Explorer exists on NT 3.1. Notify this process's top-level
 windows so association observers can invalidate their cached state. */if(event&SHCNE_ASSOCCHANGED){iconGeneration++;clearAssociations();SendMessageA(HWND_BROADCAST,WM_WININICHANGE,0,(LPARAM)"Software\\Classes");}}
SHELL_BIND(SHChangeNotify,16);
HRESULT WINAPI SHGetDesktopFolder(IShellFolder **out){__typeof__(&SHGetDesktopFolder) fn=SHELL_NATIVE(SHGetDesktopFolder);if(fn)return fn(out);return Z7ShellFolder(Z7_DESKTOP,L"",&IID_IShellFolder,(void**)out);}
SHELL_BIND(SHGetDesktopFolder,4);

#endif /* !Z7_SHELL_PATHS_ONLY */
