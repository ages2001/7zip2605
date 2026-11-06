/* OLE services used by 7-Zip on NT 3.1. Later systems retain their native OLE.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#define BIND(name, bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes) = legacy_##name
static HMODULE oleModule(void){HMODULE m;DWORD v=GetVersion();if(!(v&0x80000000UL)&&LOBYTE(LOWORD(v))==3&&HIBYTE(LOWORD(v))<50)return NULL;m=GetModuleHandleA("ole32.dll");return m?m:LoadLibraryA("ole32.dll");}
#define NATIVE(name) ((__typeof__(&name))GetProcAddress(oleModule(),#name))
static void *allocate(SIZE_T n){return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,n);}
static void release(void *p){if(p)HeapFree(GetProcessHeap(),0,p);}
static BOOL guidEqual(REFGUID a,REFGUID b){const DWORD *x=(const DWORD*)a,*y=(const DWORD*)b;return x[0]==y[0]&&x[1]==y[1]&&x[2]==y[2]&&x[3]==y[3];}
static DWORD tlsIndex=TLS_OUT_OF_INDEXES;static LONG tlsLock;
typedef struct Apartment{ULONG count,oleCount;} Apartment;
static Apartment *apartment(BOOL create){Apartment *p;while(InterlockedExchange(&tlsLock,1))Sleep(0);if(tlsIndex==TLS_OUT_OF_INDEXES&&create)tlsIndex=TlsAlloc();InterlockedExchange(&tlsLock,0);if(tlsIndex==TLS_OUT_OF_INDEXES)return NULL;p=TlsGetValue(tlsIndex);if(!p&&create){p=allocate(sizeof(*p));if(p&&!TlsSetValue(tlsIndex,p)){release(p);return NULL;}}return p;}
static HRESULT WINAPI legacy_CoInitialize(LPVOID reserved){__typeof__(&CoInitialize) fn=NATIVE(CoInitialize);Apartment *p;if(fn)return fn(reserved);if(reserved)return E_INVALIDARG;p=apartment(TRUE);if(!p)return E_OUTOFMEMORY;return p->count++?S_FALSE:S_OK;}
BIND(CoInitialize,4);
static void WINAPI legacy_CoUninitialize(void){__typeof__(&CoUninitialize) fn=NATIVE(CoUninitialize);Apartment *p;if(fn){fn();return;}p=apartment(FALSE);if(p&&p->count&&!--p->count){TlsSetValue(tlsIndex,NULL);release(p);}}
BIND(CoUninitialize,0);
static HRESULT WINAPI legacy_OleInitialize(LPVOID reserved){__typeof__(&OleInitialize) fn=NATIVE(OleInitialize);Apartment *p;HRESULT hr;if(fn)return fn(reserved);hr=legacy_CoInitialize(reserved);if(FAILED(hr))return hr;p=apartment(FALSE);return p->oleCount++?S_FALSE:S_OK;}
BIND(OleInitialize,4);
static void WINAPI legacy_OleUninitialize(void){__typeof__(&OleUninitialize) fn=NATIVE(OleUninitialize);Apartment *p;if(fn){fn();return;}p=apartment(FALSE);if(p&&p->oleCount){p->oleCount--;legacy_CoUninitialize();}}
BIND(OleUninitialize,0);
static LPVOID WINAPI legacy_CoTaskMemAlloc(SIZE_T size){__typeof__(&CoTaskMemAlloc) fn=NATIVE(CoTaskMemAlloc);return fn?fn(size):HeapAlloc(GetProcessHeap(),0,size?size:1);}
BIND(CoTaskMemAlloc,4);
static void WINAPI legacy_CoTaskMemFree(LPVOID p){__typeof__(&CoTaskMemFree) fn=NATIVE(CoTaskMemFree);if(fn)fn(p);else release(p);}
BIND(CoTaskMemFree,4);
/* Loaded in-process servers remain loaded: an outstanding interface may still
   execute their code after CoUninitialize. Never unload a live class factory. */
static HRESULT WINAPI legacy_CoCreateInstance(REFCLSID clsid,LPUNKNOWN outer,DWORD context,REFIID iid,LPVOID *out){
 __typeof__(&CoCreateInstance) fn=NATIVE(CoCreateInstance);char key[96],path[32768];HKEY reg;DWORD type,size=sizeof(path);LONG error;HMODULE module;IClassFactory *factory;HRESULT hr;
 typedef HRESULT (WINAPI *GetClass)(REFCLSID,REFIID,void**);GetClass getClass;
 if(fn)return fn(clsid,outer,context,iid,out);if(!out)return E_POINTER;*out=NULL;if(!clsid||!iid)return E_INVALIDARG;if(!apartment(FALSE)||!apartment(FALSE)->count)return CO_E_NOTINITIALIZED;
 if(!(context&CLSCTX_INPROC_SERVER))return E_NOTIMPL;
 wsprintfA(key,"CLSID\\{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\\InprocServer32",clsid->Data1,clsid->Data2,clsid->Data3,clsid->Data4[0],clsid->Data4[1],clsid->Data4[2],clsid->Data4[3],clsid->Data4[4],clsid->Data4[5],clsid->Data4[6],clsid->Data4[7]);
 error=RegOpenKeyExA(HKEY_CLASSES_ROOT,key,0,KEY_READ,&reg);if(error)return error==ERROR_FILE_NOT_FOUND?REGDB_E_CLASSNOTREG:HRESULT_FROM_WIN32(error);
 error=RegQueryValueExA(reg,NULL,NULL,&type,(BYTE*)path,&size);RegCloseKey(reg);if(error)return HRESULT_FROM_WIN32(error);if((type!=REG_SZ&&type!=REG_EXPAND_SZ)||!size||size>sizeof(path)||path[size-1])return REGDB_E_READREGDB;
 if(type==REG_EXPAND_SZ){char *expanded=allocate(sizeof(path));DWORD n;if(!expanded)return E_OUTOFMEMORY;n=ExpandEnvironmentStringsA(path,expanded,sizeof(path));if(!n||n>sizeof(path)){release(expanded);return REGDB_E_READREGDB;}CopyMemory(path,expanded,n);release(expanded);}
 module=LoadLibraryA(path);if(!module)return HRESULT_FROM_WIN32(GetLastError());getClass=(GetClass)GetProcAddress(module,"DllGetClassObject");if(!getClass){FreeLibrary(module);return CO_E_DLLNOTFOUND;}
 hr=getClass(clsid,&IID_IClassFactory,(void**)&factory);if(FAILED(hr)){FreeLibrary(module);return hr;}hr=IClassFactory_CreateInstance(factory,outer,iid,out);IClassFactory_Release(factory);return hr;
}
BIND(CoCreateInstance,20);
static void WINAPI legacy_ReleaseStgMedium(LPSTGMEDIUM medium){
 __typeof__(&ReleaseStgMedium) fn=NATIVE(ReleaseStgMedium);if(fn){fn(medium);return;}if(!medium)return;
 if(medium->pUnkForRelease)IUnknown_Release(medium->pUnkForRelease);
 else switch(medium->tymed){
 case TYMED_HGLOBAL:if(medium->hGlobal)GlobalFree(medium->hGlobal);break;
 case TYMED_FILE:if(medium->lpszFileName){DeleteFileW(medium->lpszFileName);legacy_CoTaskMemFree(medium->lpszFileName);}break;
 case TYMED_ISTREAM:if(medium->pstm)IStream_Release(medium->pstm);break;
 case TYMED_ISTORAGE:if(medium->pstg)IStorage_Release(medium->pstg);break;
 case TYMED_GDI:if(medium->hBitmap)DeleteObject(medium->hBitmap);break;
 case TYMED_MFPICT:if(medium->hMetaFilePict){METAFILEPICT *p=GlobalLock(medium->hMetaFilePict);if(p){DeleteMetaFile(p->hMF);GlobalUnlock(medium->hMetaFilePict);}GlobalFree(medium->hMetaFilePict);}break;
 case TYMED_ENHMF:if(medium->hEnhMetaFile)DeleteEnhMetaFile(medium->hEnhMetaFile);break;
 }
 ZeroMemory(medium,sizeof(*medium));
}
BIND(ReleaseStgMedium,4);
/* IDataObject used when File Manager or another legacy sender delivers HDROP. */
typedef struct DropData{IDataObject iface;LONG refs;HGLOBAL data;DWORD performed;} DropData;
static HGLOBAL duplicateGlobal(HGLOBAL h){SIZE_T size=GlobalSize(h);void *source,*dest;HGLOBAL copy;if(!size)return NULL;source=GlobalLock(h);if(!source)return NULL;copy=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,size);dest=copy?GlobalLock(copy):NULL;if(dest){CopyMemory(dest,source,size);GlobalUnlock(copy);}else{if(copy)GlobalFree(copy);copy=NULL;}GlobalUnlock(h);return copy;}
static HRESULT STDMETHODCALLTYPE dataQuery(IDataObject *self,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;if(!guidEqual(iid,&IID_IUnknown)&&!guidEqual(iid,&IID_IDataObject))return E_NOINTERFACE;*out=self;IDataObject_AddRef(self);return S_OK;}
static ULONG STDMETHODCALLTYPE dataAdd(IDataObject *self){return InterlockedIncrement(&((DropData*)self)->refs);}
static ULONG STDMETHODCALLTYPE dataRelease(IDataObject *self){DropData *p=(DropData*)self;ULONG n=InterlockedDecrement(&p->refs);if(!n){GlobalFree(p->data);release(p);}return n;}
static HRESULT STDMETHODCALLTYPE dataCan(IDataObject *self,FORMATETC *format){(void)self;if(!format)return E_INVALIDARG;if(format->cfFormat!=CF_HDROP)return DV_E_FORMATETC;if(format->dwAspect!=DVASPECT_CONTENT)return DV_E_DVASPECT;if(!(format->tymed&TYMED_HGLOBAL))return DV_E_TYMED;if(format->lindex!=-1)return DV_E_LINDEX;if(format->ptd)return DV_E_DVTARGETDEVICE;return S_OK;}
static HRESULT STDMETHODCALLTYPE dataGet(IDataObject *self,FORMATETC *format,STGMEDIUM *medium){HRESULT hr;if(!medium)return E_POINTER;ZeroMemory(medium,sizeof(*medium));hr=dataCan(self,format);if(FAILED(hr))return hr;medium->hGlobal=duplicateGlobal(((DropData*)self)->data);if(!medium->hGlobal)return E_OUTOFMEMORY;medium->tymed=TYMED_HGLOBAL;return S_OK;}
static HRESULT STDMETHODCALLTYPE dataHere(IDataObject *s,FORMATETC *f,STGMEDIUM *m){(void)s;(void)f;(void)m;return DATA_E_FORMATETC;}
static HRESULT STDMETHODCALLTYPE dataCanonical(IDataObject *s,FORMATETC *f,FORMATETC *out){(void)s;if(!f||!out)return E_POINTER;*out=*f;out->ptd=NULL;return DATA_S_SAMEFORMATETC;}
static HRESULT STDMETHODCALLTYPE dataSet(IDataObject *s,FORMATETC *f,STGMEDIUM *m,BOOL own){DWORD *value,effect;if(!f||!m)return E_POINTER;if(!f->cfFormat||f->cfFormat!=RegisterClipboardFormatA("Performed DropEffect"))return DV_E_FORMATETC;if(f->dwAspect!=DVASPECT_CONTENT||f->lindex!=-1||f->ptd)return DV_E_FORMATETC;if(!(f->tymed&TYMED_HGLOBAL)||m->tymed!=TYMED_HGLOBAL||GlobalSize(m->hGlobal)<sizeof(DWORD))return DV_E_TYMED;value=GlobalLock(m->hGlobal);if(!value)return E_INVALIDARG;effect=*value;GlobalUnlock(m->hGlobal);if(effect&~(DROPEFFECT_COPY|DROPEFFECT_MOVE))return E_INVALIDARG;((DropData*)s)->performed=effect;if(own)legacy_ReleaseStgMedium(m);return S_OK;}
typedef struct Formats{IEnumFORMATETC iface;LONG refs;ULONG cursor;} Formats;
static HRESULT STDMETHODCALLTYPE formatsQuery(IEnumFORMATETC *s,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;if(!guidEqual(iid,&IID_IUnknown)&&!guidEqual(iid,&IID_IEnumFORMATETC))return E_NOINTERFACE;*out=s;IEnumFORMATETC_AddRef(s);return S_OK;}
static ULONG STDMETHODCALLTYPE formatsAdd(IEnumFORMATETC *s){return InterlockedIncrement(&((Formats*)s)->refs);}
static ULONG STDMETHODCALLTYPE formatsRelease(IEnumFORMATETC *s){ULONG n=InterlockedDecrement(&((Formats*)s)->refs);if(!n)release(s);return n;}
static HRESULT STDMETHODCALLTYPE formatsNext(IEnumFORMATETC *s,ULONG count,FORMATETC *out,ULONG *fetched){Formats *p=(Formats*)s;if(fetched)*fetched=0;if(!out||(!fetched&&count!=1))return E_POINTER;if(!count)return S_OK;if(p->cursor)return S_FALSE;ZeroMemory(out,sizeof(*out));out->cfFormat=CF_HDROP;out->dwAspect=DVASPECT_CONTENT;out->lindex=-1;out->tymed=TYMED_HGLOBAL;p->cursor=1;if(fetched)*fetched=1;return count==1?S_OK:S_FALSE;}
static HRESULT STDMETHODCALLTYPE formatsSkip(IEnumFORMATETC *s,ULONG n){Formats *p=(Formats*)s;if(!n)return S_OK;if(p->cursor)return S_FALSE;p->cursor=1;return n==1?S_OK:S_FALSE;}
static HRESULT STDMETHODCALLTYPE formatsReset(IEnumFORMATETC *s){((Formats*)s)->cursor=0;return S_OK;}
static HRESULT STDMETHODCALLTYPE formatsClone(IEnumFORMATETC *s,IEnumFORMATETC **out){Formats *p;if(!out)return E_POINTER;*out=NULL;p=allocate(sizeof(*p));if(!p)return E_OUTOFMEMORY;*p=*(Formats*)s;p->refs=1;*out=&p->iface;return S_OK;}
static IEnumFORMATETCVtbl formatsVtable={formatsQuery,formatsAdd,formatsRelease,formatsNext,formatsSkip,formatsReset,formatsClone};
static HRESULT STDMETHODCALLTYPE dataEnum(IDataObject *s,DWORD direction,IEnumFORMATETC **out){Formats *p;(void)s;if(!out)return E_POINTER;*out=NULL;if(direction!=DATADIR_GET)return E_NOTIMPL;p=allocate(sizeof(*p));if(!p)return E_OUTOFMEMORY;p->iface.lpVtbl=&formatsVtable;p->refs=1;*out=&p->iface;return S_OK;}
static HRESULT STDMETHODCALLTYPE dataAdvise(IDataObject *s,FORMATETC *f,DWORD flags,IAdviseSink *sink,DWORD *connection){(void)s;(void)f;(void)flags;(void)sink;if(connection)*connection=0;return OLE_E_ADVISENOTSUPPORTED;}
static HRESULT STDMETHODCALLTYPE dataUnadvise(IDataObject *s,DWORD connection){(void)s;(void)connection;return OLE_E_ADVISENOTSUPPORTED;}
static HRESULT STDMETHODCALLTYPE dataEnumAdvise(IDataObject *s,IEnumSTATDATA **out){(void)s;if(out)*out=NULL;return OLE_E_ADVISENOTSUPPORTED;}
static IDataObjectVtbl dataVtable={dataQuery,dataAdd,dataRelease,dataGet,dataHere,dataCan,dataCanonical,dataSet,dataEnum,dataAdvise,dataUnadvise,dataEnumAdvise};
static DropData *makeData(HGLOBAL h){DropData *p=allocate(sizeof(*p));if(!p)return NULL;p->data=duplicateGlobal(h);if(!p->data){release(p);return NULL;}p->iface.lpVtbl=&dataVtable;p->refs=1;p->performed=0;return p;}
#include "file_drop_protocol.h"
#define DROP_PROPERTY "Z7.NT31.DropTarget"
typedef struct DropRegistration{IDropTarget *target;WNDPROC previous;BOOL accepted;} DropRegistration;
static LRESULT CALLBACK dropWindow(HWND,UINT,WPARAM,LPARAM);
static DWORD keys(void){DWORD k=0;if(GetAsyncKeyState(VK_LBUTTON)&0x8000)k|=MK_LBUTTON;if(GetAsyncKeyState(VK_RBUTTON)&0x8000)k|=MK_RBUTTON;if(GetAsyncKeyState(VK_MBUTTON)&0x8000)k|=MK_MBUTTON;if(GetAsyncKeyState(VK_SHIFT)&0x8000)k|=MK_SHIFT;if(GetAsyncKeyState(VK_CONTROL)&0x8000)k|=MK_CONTROL;return k;}
static BOOL fileDropBytesValid(const BYTE *bytes,DWORD size)
{
 const Z7DropFilesHeader *drop;
 DWORD i,step;
 if(size<sizeof(Z7DropFilesHeader)+2)return FALSE;
 drop=(const Z7DropFilesHeader *)bytes;step=drop->fWide?2:1;
 if(drop->pFiles<sizeof(Z7DropFilesHeader)||drop->pFiles>size-2*step||(step==2&&(drop->pFiles&1)))return FALSE;
 if(!bytes[drop->pFiles]&&(!drop->fWide||!bytes[drop->pFiles+1]))return FALSE;
 for(i=drop->pFiles;i<=size-2*step;i+=step)
  if(!bytes[i]&&(!drop->fWide||!bytes[i+1])&&!bytes[i+step]&&(!drop->fWide||!bytes[i+step+1]))return TRUE;
 return FALSE;
}
static LRESULT receiveFileDrop(HWND window,DropRegistration *registration,const COPYDATASTRUCT *copy)
{
 const Z7FileDropPacket *packet;
 HGLOBAL global;
 void *bytes;
 DropData *data;
 IDropTarget *target;
 HRESULT hr;
 DWORD effect,reply=Z7_DROP_REPLY_TAG;
 if(!copy||copy->dwData!=Z7_DROP_PROTOCOL_TAG||copy->cbData<sizeof(Z7FileDropPacket)
    ||copy->cbData>Z7_DROP_MAX_BYTES||!copy->lpData)return 0;
 packet=(const Z7FileDropPacket *)copy->lpData;
 if(packet->version!=1||packet->dataSize!=copy->cbData-sizeof(*packet)
    ||!fileDropBytesValid((const BYTE *)(packet+1),packet->dataSize))return 0;
 effect=fileDropEffect(packet->effects&(DWORD)(ULONG_PTR)GetPropA(window,Z7_DROP_PROTOCOL_PROPERTY),packet->keyState);
 if(!effect)return reply;
 global=GlobalAlloc(GMEM_MOVEABLE,packet->dataSize);bytes=global?GlobalLock(global):NULL;
 if(!bytes){if(global)GlobalFree(global);return reply;}
 CopyMemory(bytes,packet+1,packet->dataSize);GlobalUnlock(global);data=makeData(global);GlobalFree(global);
 if(!data)return reply;
 target=registration->target;IDropTarget_AddRef(target);
 hr=IDropTarget_DragEnter(target,&data->iface,packet->keyState,packet->point,&effect);
 if(SUCCEEDED(hr)){
  effect&=packet->effects;
  if(effect){
   hr=IDropTarget_Drop(target,&data->iface,packet->keyState,packet->point,&effect);
   if(SUCCEEDED(hr)&&((effect|data->performed)&packet->effects))
    reply|=Z7_DROP_REPLY_ACCEPTED|(effect&packet->effects&3)|((data->performed&packet->effects&3)<<8);
  }else IDropTarget_DragLeave(target);
 }
 IDropTarget_Release(target);IDataObject_Release(&data->iface);return reply;
}
static LRESULT CALLBACK dropWindow(HWND window,UINT message,WPARAM wp,LPARAM lp){
 DropRegistration *r=(DropRegistration*)GetPropA(window,DROP_PROPERTY);WNDPROC previous=r?r->previous:DefWindowProcA;
 if(message==fileDropQueryMessage()&&wp==Z7_DROP_PROTOCOL_TAG&&r&&r->target)
  return Z7_DROP_REPLY_TAG|((DWORD)(ULONG_PTR)GetPropA(window,Z7_DROP_PROTOCOL_PROPERTY)&3);
 if(message==WM_NCDESTROY&&r){RemovePropA(window,DROP_PROPERTY);RemovePropA(window,Z7_DROP_PROTOCOL_PROPERTY);if(r->target)IDropTarget_Release(r->target);release(r);return CallWindowProcA(previous,window,message,wp,lp);}
 if(message==WM_COPYDATA&&r&&r->target&&GetPropA(window,Z7_DROP_PROTOCOL_PROPERTY)){
  const COPYDATASTRUCT *copy=(const COPYDATASTRUCT *)lp;
  if(copy&&copy->dwData==Z7_DROP_PROTOCOL_TAG)return receiveFileDrop(window,r,copy);
 }
 if(message==WM_DROPFILES&&r&&r->target){IDropTarget *target=r->target;DropData *data;POINT p;POINTL pt;BOOL accepted=FALSE;DWORD effect=DROPEFFECT_COPY;IDropTarget_AddRef(target);data=makeData((HGLOBAL)wp);DragFinish((HDROP)wp);GetCursorPos(&p);pt.x=p.x;pt.y=p.y;
  if(data){HRESULT hr=IDropTarget_DragEnter(target,&data->iface,keys(),pt,&effect);if(SUCCEEDED(hr)){if(effect&DROPEFFECT_COPY){effect=DROPEFFECT_COPY;accepted=SUCCEEDED(IDropTarget_Drop(target,&data->iface,keys(),pt,&effect))&&(effect&DROPEFFECT_COPY);}else IDropTarget_DragLeave(target);}IDataObject_Release(&data->iface);}IDropTarget_Release(target);return accepted;}
 return CallWindowProcA(previous,window,message,wp,lp);
}
static HRESULT WINAPI legacy_RegisterDragDrop(HWND window,LPDROPTARGET target){
 __typeof__(&RegisterDragDrop) fn=NATIVE(RegisterDragDrop);DropRegistration *r;DWORD pid;
 if(fn)return fn(window,target);if(!target)return E_INVALIDARG;if(!IsWindow(window))return DRAGDROP_E_INVALIDHWND;
 if(GetWindowThreadProcessId(window,&pid)!=GetCurrentThreadId()||pid!=GetCurrentProcessId())return RPC_E_WRONG_THREAD;
 if(!apartment(FALSE)||!apartment(FALSE)->oleCount)return CO_E_NOTINITIALIZED;
 r=(DropRegistration*)GetPropA(window,DROP_PROPERTY);if(r&&r->target)return DRAGDROP_E_ALREADYREGISTERED;
 if(!r){r=allocate(sizeof(*r));if(!r)return E_OUTOFMEMORY;r->accepted=(GetWindowLongA(window,GWL_EXSTYLE)&WS_EX_ACCEPTFILES)!=0;
  r->previous=(WNDPROC)GetWindowLongA(window,GWL_WNDPROC);if(!SetPropA(window,DROP_PROPERTY,(HANDLE)r)){release(r);return E_OUTOFMEMORY;}SetLastError(0);if(!SetWindowLongA(window,GWL_WNDPROC,(LONG)dropWindow)&&GetLastError()){RemovePropA(window,DROP_PROPERTY);release(r);return E_FAIL;}}
 r->target=target;IDropTarget_AddRef(target);DragAcceptFiles(window,TRUE);return S_OK;
}
BIND(RegisterDragDrop,8);
static HRESULT WINAPI legacy_RevokeDragDrop(HWND window){
 __typeof__(&RevokeDragDrop) fn=NATIVE(RevokeDragDrop);DropRegistration *r;IDropTarget *target;DWORD pid;
 if(fn)return fn(window);if(!IsWindow(window))return DRAGDROP_E_INVALIDHWND;if(GetWindowThreadProcessId(window,&pid)!=GetCurrentThreadId()||pid!=GetCurrentProcessId())return RPC_E_WRONG_THREAD;
 r=(DropRegistration*)GetPropA(window,DROP_PROPERTY);if(!r||!r->target)return DRAGDROP_E_NOTREGISTERED;target=r->target;r->target=NULL;RemovePropA(window,Z7_DROP_PROTOCOL_PROPERTY);DragAcceptFiles(window,r->accepted);
 if((WNDPROC)GetWindowLongA(window,GWL_WNDPROC)==dropWindow){SetWindowLongA(window,GWL_WNDPROC,(LONG)r->previous);RemovePropA(window,DROP_PROPERTY);release(r);}IDropTarget_Release(target);return S_OK;
}
BIND(RevokeDragDrop,4);
static IDropTarget *targetAt(POINT point,HWND *legacy){HWND w=WindowFromPoint(point);*legacy=NULL;while(w){DWORD pid;DWORD thread=GetWindowThreadProcessId(w,&pid);if(pid==GetCurrentProcessId()&&thread==GetCurrentThreadId()){DropRegistration *r=(DropRegistration*)GetPropA(w,DROP_PROPERTY);if(r&&r->target){IDropTarget_AddRef(r->target);return r->target;}}if(!*legacy&&(GetWindowLongA(w,GWL_EXSTYLE)&WS_EX_ACCEPTFILES))*legacy=w;w=GetParent(w);}return NULL;}
static HRESULT sendFileDrop(HWND window,HWND sender,IDataObject *data,DWORD state,POINTL point,DWORD effect,DWORD *result)
{
 FORMATETC format={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
 STGMEDIUM medium;
 COPYDATASTRUCT copy;
 Z7FileDropPacket *packet;
 const void *bytes;
 SIZE_T size;
 DWORD reply;
 HRESULT hr=IDataObject_GetData(data,&format,&medium);
 if(FAILED(hr))return hr;
 if(medium.tymed!=TYMED_HGLOBAL){legacy_ReleaseStgMedium(&medium);return DV_E_TYMED;}
 size=GlobalSize(medium.hGlobal);bytes=GlobalLock(medium.hGlobal);
 if(!bytes||size>Z7_DROP_MAX_BYTES-sizeof(*packet)||!fileDropBytesValid(bytes,(DWORD)size)){
  if(bytes)GlobalUnlock(medium.hGlobal);legacy_ReleaseStgMedium(&medium);return E_INVALIDARG;
 }
 packet=allocate(sizeof(*packet)+size);
 if(packet){packet->version=1;packet->effects=effect;packet->keyState=state;packet->point=point;packet->dataSize=(DWORD)size;CopyMemory(packet+1,bytes,size);}
 GlobalUnlock(medium.hGlobal);legacy_ReleaseStgMedium(&medium);
 if(!packet)return E_OUTOFMEMORY;
 copy.dwData=Z7_DROP_PROTOCOL_TAG;copy.cbData=(DWORD)(sizeof(*packet)+size);copy.lpData=packet;
 /* USER marshals WM_COPYDATA and keeps its bytes valid for the entire call.
    The recipient completes copying/moving before source temp files expire. */
 reply=(DWORD)SendMessageA(window,WM_COPYDATA,(WPARAM)sender,(LPARAM)&copy);release(packet);
 if((reply&0xFF000000UL)!=Z7_DROP_REPLY_TAG||!(reply&Z7_DROP_REPLY_ACCEPTED))return DRAGDROP_S_CANCEL;
 *result=reply&effect&3;
 fileDropPerformed(data,(reply>>8)&effect&3);
 return DRAGDROP_S_DROP;
}
static HRESULT WINAPI legacy_DoDragDrop(LPDATAOBJECT data,LPDROPSOURCE source,DWORD allowed,LPDWORD result){
 __typeof__(&DoDragDrop) fn=NATIVE(DoDragDrop);HWND capture,oldCapture,legacy=NULL;IDropTarget *target=NULL;HRESULT hr=S_OK;BOOL escape=FALSE,quit=FALSE;int quitCode=0;DWORD effect=0;
 if(fn)return fn(data,source,allowed,result);if(!data||!source||!result)return E_INVALIDARG;*result=0;if(!apartment(FALSE)||!apartment(FALSE)->oleCount)return CO_E_NOTINITIALIZED;
 capture=CreateWindowA("STATIC","",WS_POPUP,0,0,0,0,NULL,NULL,GetModuleHandleA(NULL),NULL);if(!capture)return E_OUTOFMEMORY;
 IDataObject_AddRef(data);IDropSource_AddRef(source);oldCapture=GetCapture();SetCapture(capture);
 for(;;){POINT point;POINTL pt;DWORD state;IDropTarget *next;MSG message;HRESULT feedback;
  while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)){if(message.message==WM_QUIT){quit=TRUE;quitCode=(int)message.wParam;escape=TRUE;break;}if((message.message==WM_KEYDOWN||message.message==WM_SYSKEYDOWN)&&message.wParam==VK_ESCAPE)escape=TRUE;
   if((message.message>=WM_MOUSEFIRST&&message.message<=WM_MOUSELAST)||(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE)||(message.message==WM_KEYUP&&message.wParam==VK_ESCAPE))continue;TranslateMessage(&message);DispatchMessageA(&message);}
  if(GetAsyncKeyState(VK_ESCAPE)&0x8000)escape=TRUE;if(GetCapture()!=capture)escape=TRUE;
  GetCursorPos(&point);pt.x=point.x;pt.y=point.y;state=keys();next=targetAt(point,&legacy);
  if(next!=target){if(target){IDropTarget_DragLeave(target);IDropTarget_Release(target);}target=next;effect=allowed;if(target){hr=IDropTarget_DragEnter(target,data,state,pt,&effect);if(FAILED(hr)){IDropTarget_Release(target);target=NULL;break;}}}
  else{if(next)IDropTarget_Release(next);effect=allowed;if(target&&FAILED(IDropTarget_DragOver(target,state,pt,&effect)))effect=0;}
  if(!target){FORMATETC format={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};DWORD supported=fileDropCapabilities(legacy);effect=legacy&&SUCCEEDED(IDataObject_QueryGetData(data,&format))?(supported?fileDropEffect(allowed&supported,state):allowed&DROPEFFECT_COPY):0;}
  effect&=allowed;feedback=IDropSource_GiveFeedback(source,effect);if(FAILED(feedback)){hr=feedback;break;}if(feedback==DRAGDROP_S_USEDEFAULTCURSORS)SetCursor(LoadCursorA(NULL,effect?IDC_ARROW:IDC_NO));
  hr=IDropSource_QueryContinueDrag(source,escape,state);
  if(hr==DRAGDROP_S_DROP){if(target&&effect){hr=IDropTarget_Drop(target,data,state,pt,&effect);if(SUCCEEDED(hr)){*result=effect&allowed;hr=DRAGDROP_S_DROP;}IDropTarget_Release(target);target=NULL;}
   else if(legacy&&effect&&fileDropCapabilities(legacy))hr=sendFileDrop(legacy,capture,data,state,pt,effect,result);
   else if(legacy&&effect){FORMATETC format={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM medium;hr=IDataObject_GetData(data,&format,&medium);if(SUCCEEDED(hr)){HGLOBAL copy=medium.tymed==TYMED_HGLOBAL?duplicateGlobal(medium.hGlobal):NULL;legacy_ReleaseStgMedium(&medium);if(!copy)hr=E_OUTOFMEMORY;else if(!IsWindow(legacy)){GlobalFree(copy);hr=DRAGDROP_S_CANCEL;}else{DWORD pid;LRESULT delivered;BOOL ours=GetPropA(legacy,DROP_PROPERTY)!=NULL;GetWindowThreadProcessId(legacy,&pid);
       if(pid!=GetCurrentProcessId()){
        /* WM_DROPFILES is marshalled by PostMessage, not SendMessage. Use
           USER's global-memory transfer first, then wait for the recipient
           to finish before 7-Zip can remove extracted temporary files.
           WM_COPYGLOBALDATA consumes the sender's global allocation. */
        copy=(HGLOBAL)SendMessageA(legacy,0x0049,(WPARAM)GlobalSize(copy),(LPARAM)copy);
       }
       if(!copy)hr=E_OUTOFMEMORY;
       else{delivered=SendMessageA(legacy,WM_DROPFILES,(WPARAM)copy,0);if(ours&&!delivered)hr=DRAGDROP_S_CANCEL;else{*result=DROPEFFECT_COPY;hr=DRAGDROP_S_DROP;}}
      }}}
   else hr=DRAGDROP_S_CANCEL;break;}
  if(hr!=S_OK)break;Sleep(10);
 }
 if(target){IDropTarget_DragLeave(target);IDropTarget_Release(target);}if(GetCapture()==capture)ReleaseCapture();if(oldCapture&&IsWindow(oldCapture))SetCapture(oldCapture);DestroyWindow(capture);IDropSource_Release(source);IDataObject_Release(data);if(quit)PostQuitMessage(quitCode);return hr;
}
BIND(DoDragDrop,16);
