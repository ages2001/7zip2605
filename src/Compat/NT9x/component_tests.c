/* End-to-end auxiliary GUI, SFX and Explorer extension tests.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <string.h>
#include "setup_shortcuts.h"
static HANDLE report;static unsigned failures;
static const char payload[]="7-Zip component round trip\r\n";
static char archive[MAX_PATH];
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);}
static void check(BOOL ok,const char *s){line(ok?"PASS":"FAIL");line(s);if(!ok)failures++;}
static DWORD run(const char *exe,const char *args){STARTUPINFOA si;PROCESS_INFORMATION pi;char cmd[3*MAX_PATH];DWORD code=0xffffffff;ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);wsprintfA(cmd,"\"%s\" %s",exe,args);line(cmd);if(!CreateProcessA(exe,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi))return GetLastError();if(WaitForSingleObject(pi.hProcess,60000)==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return code;}
static BOOL same(const char *path){HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);char buffer[128];DWORD n=0;if(file==INVALID_HANDLE_VALUE)return FALSE;ReadFile(file,buffer,sizeof(buffer),&n,NULL);CloseHandle(file);return n==sizeof(payload)-1&&!memcmp(buffer,payload,n);}
static BOOL append(HANDLE out,const char *path){HANDLE in=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);BYTE buffer[4096];DWORD n,w;BOOL ok=FALSE;if(in==INVALID_HANDLE_VALUE)return FALSE;for(;;){if(!ReadFile(in,buffer,sizeof(buffer),&n,NULL))break;if(!n){ok=TRUE;break;}if(!WriteFile(out,buffer,n,&w,NULL)||w!=n)break;}CloseHandle(in);return ok;}
static BOOL join(const char *stub,const char *name){HANDLE out=CreateFileA(name,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);BOOL ok;if(out==INVALID_HANDLE_VALUE)return FALSE;ok=append(out,stub)&&append(out,archive);CloseHandle(out);return ok;}
static BOOL equal(REFIID a,REFIID b){return !memcmp(a,b,sizeof(GUID));}
static ULONG STDMETHODCALLTYPE add(IDataObject *self){return 2;}
static ULONG STDMETHODCALLTYPE release(IDataObject *self){return 1;}
static HRESULT STDMETHODCALLTYPE query(IDataObject *self,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;if(!equal(iid,&IID_IUnknown)&&!equal(iid,&IID_IDataObject))return E_NOINTERFACE;*out=self;return S_OK;}
static HRESULT STDMETHODCALLTYPE get(IDataObject *self,FORMATETC *format,STGMEDIUM *medium){HGLOBAL h;DROPFILES *p;SIZE_T size;if(!format||!medium)return E_POINTER;if(format->cfFormat!=CF_HDROP||!(format->tymed&TYMED_HGLOBAL))return DV_E_FORMATETC;size=sizeof(DROPFILES)+lstrlenA(archive)+2;h=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,size);if(!h)return E_OUTOFMEMORY;p=GlobalLock(h);if(!p){GlobalFree(h);return E_OUTOFMEMORY;}p->pFiles=sizeof(*p);lstrcpyA((char*)p+sizeof(*p),archive);GlobalUnlock(h);ZeroMemory(medium,sizeof(*medium));medium->tymed=TYMED_HGLOBAL;medium->hGlobal=h;return S_OK;}
static HRESULT STDMETHODCALLTYPE here(IDataObject*s,FORMATETC*f,STGMEDIUM*m){return DATA_E_FORMATETC;}
static HRESULT STDMETHODCALLTYPE queryGet(IDataObject*s,FORMATETC*f){return f&&f->cfFormat==CF_HDROP&&(f->tymed&TYMED_HGLOBAL)?S_OK:DV_E_FORMATETC;}
static HRESULT STDMETHODCALLTYPE canonical(IDataObject*s,FORMATETC*f,FORMATETC*out){if(out)out->ptd=NULL;return DATA_S_SAMEFORMATETC;}
static HRESULT STDMETHODCALLTYPE set(IDataObject*s,FORMATETC*f,STGMEDIUM*m,BOOL own){return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE formats(IDataObject*s,DWORD direction,IEnumFORMATETC**out){if(out)*out=NULL;return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE advise(IDataObject*s,FORMATETC*f,DWORD flags,IAdviseSink*sink,DWORD*id){return OLE_E_ADVISENOTSUPPORTED;}
static HRESULT STDMETHODCALLTYPE unadvise(IDataObject*s,DWORD id){return OLE_E_ADVISENOTSUPPORTED;}
static HRESULT STDMETHODCALLTYPE advises(IDataObject*s,IEnumSTATDATA**out){if(out)*out=NULL;return OLE_E_ADVISENOTSUPPORTED;}
static IDataObjectVtbl dataTable={query,add,release,get,here,queryGet,canonical,set,formats,advise,unadvise,advises};
static IDataObject dataObject={&dataTable};
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show)
{
 char directory[MAX_PATH],root[MAX_PATH],exe[MAX_PATH],path[MAX_PATH];DWORD n,begin;HANDLE file;HMODULE dll;HRESULT hr;
 typedef HRESULT (WINAPI *FactoryFn)(REFCLSID,REFIID,void**);
 const CLSID classId={0x23170f69,0x40c1,0x278a,{0x10,0,0,1,0,2,0,0}};
 report=CreateFileA("C:\\COMPTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)return 2;
 check(Z7SetupDefaultPath(directory,MAX_PATH)==0,"default installed component directory");
 wsprintfA(root,"C:\\Z7C%05lX",GetTickCount()&0xfffff);check(CreateDirectoryA(root,NULL)&&SetCurrentDirectoryA(root),"isolated workspace");line(root);
 file=CreateFileA("DATA.TXT",GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);check(file!=INVALID_HANDLE_VALUE,"source file");if(file!=INVALID_HANDLE_VALUE){WriteFile(file,payload,sizeof(payload)-1,&n,NULL);CloseHandle(file);}
 wsprintfA(exe,"%s\\7zG.exe",directory);check(run(exe,"a -t7z -mx1 -mmt2 ARC.7Z DATA.TXT")==0,"7zG creates archive");wsprintfA(archive,"%s\\ARC.7Z",root);check(GetFileAttributesA(archive)!=INVALID_FILE_ATTRIBUTES,"7zG archive exists");
 check(run(exe,"x -y -oGUIOUT ARC.7Z")==0&&same("GUIOUT\\DATA.TXT"),"7zG extraction preserves exact bytes");
 wsprintfA(path,"%s\\7zCon.sfx",directory);check(join(path,"CSFX.EXE"),"assemble console SFX");wsprintfA(exe,"%s\\CSFX.EXE",root);check(run(exe,"-y -oCONOUT")==0&&same("CONOUT\\DATA.TXT"),"console SFX extracts exact bytes");
 wsprintfA(path,"%s\\7z.sfx",directory);check(join(path,"WSFX.EXE"),"assemble graphical SFX");wsprintfA(exe,"%s\\WSFX.EXE",root);check(run(exe,"-y -oSFXOUT")==0&&same("SFXOUT\\DATA.TXT"),"graphical SFX extracts exact bytes");
 CoInitialize(NULL);wsprintfA(path,"%s\\7-zip.dll",directory);dll=LoadLibraryA(path);if(!dll){char error[80];wsprintfA(error,"LoadLibrary error: %lu",GetLastError());line(error);}check(dll!=NULL,"load actual Explorer extension");
 if(dll){FactoryFn factory=(FactoryFn)GetProcAddress(dll,"DllGetClassObject");IClassFactory *creator=NULL;IContextMenu *menu=NULL;IShellExtInit *init=NULL;HMENU popup;UINT count,i,extract=~0U;char verb[128];
  hr=factory?factory(&classId,&IID_IClassFactory,(void**)&creator):E_FAIL;check(SUCCEEDED(hr)&&creator,"Explorer class factory");
  if(creator){hr=IClassFactory_CreateInstance(creator,NULL,&IID_IContextMenu,(void**)&menu);IClassFactory_Release(creator);check(SUCCEEDED(hr)&&menu,"Explorer context menu instance");}
  if(menu){hr=IContextMenu_QueryInterface(menu,&IID_IShellExtInit,(void**)&init);check(SUCCEEDED(hr)&&init,"Explorer initialization interface");if(init){check(IShellExtInit_Initialize(init,NULL,&dataObject,NULL)==S_OK,"Explorer receives CF_HDROP selection");IShellExtInit_Release(init);}
   popup=CreatePopupMenu();hr=IContextMenu_QueryContextMenu(menu,popup,0,1,1000,CMF_NORMAL);count=SUCCEEDED(hr)?HRESULT_CODE(hr):0;check(count>0,"Explorer supplies real archive commands");
   for(i=0;i<count;i++){char detail[200];verb[0]=0;hr=IContextMenu_GetCommandString(menu,i,GCS_VERBA,NULL,verb,sizeof(verb));wsprintfA(detail,"Command %u HRESULT %08lX: %s",i,(DWORD)hr,verb);line(detail);if(hr==S_OK&&strstr(verb,"ExtractHere"))extract=i;}
   check(extract!=~0U,"Explorer exposes Extract Here verb");
   if(extract!=~0U){CMINVOKECOMMANDINFO command;DeleteFileA("DATA.TXT");ZeroMemory(&command,sizeof(command));command.cbSize=sizeof(command);command.lpVerb=MAKEINTRESOURCEA(extract);command.nShow=SW_SHOWNORMAL;check(SUCCEEDED(IContextMenu_InvokeCommand(menu,&command)),"Explorer invokes actual GUI extraction");begin=GetTickCount();while(GetTickCount()-begin<30000&&!same("DATA.TXT"))Sleep(50);check(same("DATA.TXT"),"Explorer extraction preserves exact bytes");}
   DestroyMenu(popup);IContextMenu_Release(menu);
  }
  FreeLibrary(dll);
 }
 CoUninitialize();check(failures==0,"ALL");CloseHandle(report);return failures?1:0;
}
