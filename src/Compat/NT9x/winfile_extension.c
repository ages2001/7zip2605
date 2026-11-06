/* File Manager extension: receive file drops inside WinFile's process.
   Uses the documented Win32 File Manager extension and OLE interfaces.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
void *memset(void *p, int c, unsigned n) { unsigned i; for (i=0;i<n;i++) ((BYTE *)p)[i]=(BYTE)c; return p; }
void *memcpy(void *d, const void *s, unsigned n) { unsigned i; for (i=0;i<n;i++) ((BYTE *)d)[i]=((const BYTE *)s)[i]; return d; }
#include "legacy_ole.c"

/* Layouts defined by the public File Manager extension API. */
typedef struct { DWORD size; CHAR name[40]; HMENU menu; UINT delta; } LoadA;
typedef struct { DWORD size; WCHAR name[40]; HMENU menu; UINT delta; } LoadW;
typedef struct { DWORD total,free; WCHAR path[260],volume[14],share[128]; } DriveW;
typedef struct { DWORD total,free; CHAR path[260],volume[14],share[128]; } DriveA;
#define FM_GETDRIVEINFOW (WM_USER+0x211)
#define FM_REFRESH_WINDOWS (WM_USER+0x206)
#ifdef Z7_WF_TRACE
static void trace(const char *s){DWORD n;HANDLE f=CreateFileA("C:\\WFTRACE.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,0,NULL);if(f!=INVALID_HANDLE_VALUE){SetFilePointer(f,0,0,FILE_END);WriteFile(f,s,lstrlenA(s),&n,NULL);WriteFile(f,"\r\n",2,&n,NULL);FlushFileBuffers(f);CloseHandle(f);}}
#else
#define trace(s) ((void)0)
#endif
static HWND frame;
static HMENU menu;
static BOOL initialized,registered;
typedef struct { IDropTarget iface; LONG refs; DWORD allowed; } FileTarget;

#include "winfile_targets.h"

static HRESULT STDMETHODCALLTYPE query(IDropTarget *s,REFIID id,void **out)
{
  if(!out)return E_POINTER;*out=NULL;
  if(!guidEqual(id,&IID_IUnknown)&&!guidEqual(id,&IID_IDropTarget))return E_NOINTERFACE;
  *out=s;IDropTarget_AddRef(s);return S_OK;
}
static ULONG STDMETHODCALLTYPE add(IDropTarget *s){return InterlockedIncrement(&((FileTarget *)s)->refs);}
static ULONG STDMETHODCALLTYPE sub(IDropTarget *s){return InterlockedDecrement(&((FileTarget *)s)->refs);}
static HRESULT STDMETHODCALLTYPE enter(IDropTarget *s,IDataObject *d,DWORD keys,POINTL p,DWORD *effect)
{
  FORMATETC format={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
  if(!effect)return E_POINTER;
  trace("Enter");
  ((FileTarget *)s)->allowed=SUCCEEDED(IDataObject_QueryGetData(d,&format))?(*effect&(DROPEFFECT_COPY|DROPEFFECT_MOVE)):0;
  *effect=dropPointValid(p)?fileDropEffect(((FileTarget *)s)->allowed,keys):DROPEFFECT_NONE;
  return S_OK;
}
static HRESULT STDMETHODCALLTYPE over(IDropTarget *s,DWORD keys,POINTL p,DWORD *effect)
{ if(!effect)return E_POINTER;*effect=dropPointValid(p)?fileDropEffect(((FileTarget *)s)->allowed,keys):DROPEFFECT_NONE;return S_OK; }
static HRESULT STDMETHODCALLTYPE leave(IDropTarget *s){((FileTarget *)s)->allowed=0;return S_OK;}
static HRESULT STDMETHODCALLTYPE drop(IDropTarget *s,IDataObject *data,DWORD keys,POINTL p,DWORD *effect)
{
  FORMATETC format={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
  STGMEDIUM medium;
  SHFILEOPSTRUCTW op;
  WCHAR dest[262],*files,*q;
  UINT count,i,n,total=1;
  HRESULT hr;
  int error;
  DWORD selected;
  if(!effect)return E_POINTER;
  trace("Drop");
  selected=fileDropEffect(((FileTarget *)s)->allowed,keys);((FileTarget *)s)->allowed=0;
  *effect=0;
  if(!selected)return S_OK;
  if(!destinationAt(p,dest)){trace("Destination rejected");return E_INVALIDARG;}
  trace("Destination accepted");
  dest[lstrlenW(dest)+1]=0;
  hr=IDataObject_GetData(data,&format,&medium);
  if(FAILED(hr)){trace("GetData failed");return hr;}
  trace("GetData succeeded");
  if(medium.tymed!=TYMED_HGLOBAL){ReleaseStgMedium(&medium);return DV_E_TYMED;}
  count=DragQueryFileW((HDROP)medium.hGlobal,0xffffffff,NULL,0);
  if(!count){ReleaseStgMedium(&medium);return E_INVALIDARG;}
  for(i=0;i<count;i++){
    n=DragQueryFileW((HDROP)medium.hGlobal,i,NULL,0);
    if(!n||n>=260||total>0x100000-n-1){ReleaseStgMedium(&medium);return E_INVALIDARG;}
    total+=n+1;
  }
  files=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,total*sizeof(WCHAR));
  if(!files){ReleaseStgMedium(&medium);return E_OUTOFMEMORY;}
  q=files;
  for(i=0;i<count;i++){
    n=DragQueryFileW((HDROP)medium.hGlobal,i,q,total-(UINT)(q-files));
    if(!n){HeapFree(GetProcessHeap(),0,files);ReleaseStgMedium(&medium);return E_INVALIDARG;}
    q+=n+1;
  }
  ZeroMemory(&op,sizeof(op));op.hwnd=frame;op.wFunc=selected==DROPEFFECT_MOVE?FO_MOVE:FO_COPY;op.pFrom=files;op.pTo=dest;
  /* Complete the operation synchronously: the sender owns archive temp files. */
  trace(selected==DROPEFFECT_MOVE?"Starting move":"Starting copy");
  error=SHFileOperationW(&op);
  trace(error?"File operation failed":"File operation returned success");
  HeapFree(GetProcessHeap(),0,files);ReleaseStgMedium(&medium);
  if(!error&&!op.fAnyOperationsAborted){
    fileDropPerformed(data,selected);
    /* SHFileOperation already moved the files. Returning MOVE as well can
       cause a generic OLE source to delete originals for a second time. */
    *effect=selected==DROPEFFECT_MOVE?DROPEFFECT_NONE:DROPEFFECT_COPY;
    refreshDestination(p);return S_OK;
  }
  return error?HRESULT_FROM_WIN32(error):S_OK;
}
static IDropTargetVtbl table={query,add,sub,enter,over,leave,drop};
static FileTarget target={{&table},1,0};

static LONG event(HWND window,WPARAM event,LPARAM parameter,BOOL wide)
{
  if(event==100){
    if(initialized||!parameter)return FALSE;
    frame=window;
    menu=CreatePopupMenu();
    if(!menu)return FALSE;
    if(!AppendMenuA(menu,MF_STRING,1,"About file drop support...")){DestroyMenu(menu);menu=NULL;return FALSE;}
    if(FAILED(OleInitialize(NULL))){DestroyMenu(menu);menu=NULL;return FALSE;}
    initialized=TRUE;
    if(FAILED(RegisterDragDrop(frame,&target.iface))){OleUninitialize();initialized=FALSE;DestroyMenu(menu);menu=NULL;return FALSE;}
    if(!SetPropA(frame,Z7_DROP_PROTOCOL_PROPERTY,(HANDLE)(ULONG_PTR)(DROPEFFECT_COPY|DROPEFFECT_MOVE))){RevokeDragDrop(frame);OleUninitialize();initialized=FALSE;DestroyMenu(menu);menu=NULL;return FALSE;}
    registered=TRUE;trace("Registered");
    if(wide){LoadW *p=(LoadW *)parameter;p->size=sizeof(*p);lstrcpyW(p->name,L"7-Zip");p->menu=menu;}
    else {LoadA *p=(LoadA *)parameter;p->size=sizeof(*p);lstrcpyA(p->name,"7-Zip");p->menu=menu;}
    return TRUE;
  }
  if(event==101){
    RemovePropA(frame,Z7_DROP_PROTOCOL_PROPERTY);target.allowed=0;
    if(registered){RevokeDragDrop(frame);registered=FALSE;}
    if(initialized){OleUninitialize();initialized=FALSE;}
    frame=NULL;menu=NULL;return TRUE;
  }
  if(event==1){MessageBoxA(frame,"Drop files from 7-Zip onto a folder in the directory list or tree, or into the open directory window, to copy them. Hold Shift to move files, or Ctrl to copy.\r\nThe operation finishes before temporary extracted files are released. Moving extracted temporary files leaves the source archive unchanged.","7-Zip File Manager support",MB_OK);return TRUE;}
  return 0;
}
LONG WINAPI FMExtensionProc(HWND w,WPARAM e,LPARAM p){return event(w,e,p,FALSE);}
LONG WINAPI FMExtensionProcW(HWND w,WPARAM e,LPARAM p){return event(w,e,p,TRUE);}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved)
{
  (void)instance;
  /* The extension API unload event revokes the target and balances OLE.
     The fallback TLS index belongs to this DLL, not to WinFile. Release it
     when FreeLibrary unloads us, or repeated extension reloads exhaust the
     process TLS slots on NT 3.1. Process shutdown needs no cleanup. */
  if(reason==DLL_PROCESS_DETACH && !reserved && tlsIndex!=TLS_OUT_OF_INDEXES){
    release(TlsGetValue(tlsIndex));
    TlsFree(tlsIndex);
    tlsIndex=TLS_OUT_OF_INDEXES;
  }
  return TRUE;
}
