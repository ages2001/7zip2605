/* Native regression tests for NT 3.1 OLE support. No CRT required.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
void *memset(void *p,int c,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=c;return p;}
void *memcpy(void *d,const void *s,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)d)[i]=((const BYTE*)s)[i];return d;}
/* Including the implementation lets the fixture construct its HDROP data object
   without exporting a private testing function in the application. */
#include "legacy_ole.c"
static HANDLE report;static unsigned enters,overs,leaves,drops,formatsSeen;static BOOL cancelDrag,failEnter,failFeedback;
static void check(BOOL ok,const char *text){DWORD n;const char *prefix=ok?"PASS ":"FAIL ";WriteFile(report,prefix,lstrlenA(prefix),&n,NULL);WriteFile(report,text,lstrlenA(text),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);if(!ok){CloseHandle(report);ExitProcess(1);}}
typedef struct Target{IDropTarget iface;LONG refs;} Target;
static HRESULT STDMETHODCALLTYPE targetQuery(IDropTarget *s,REFIID id,void **out){if(!out)return E_POINTER;*out=NULL;if(!guidEqual(id,&IID_IUnknown)&&!guidEqual(id,&IID_IDropTarget))return E_NOINTERFACE;*out=s;IDropTarget_AddRef(s);return S_OK;}
static ULONG STDMETHODCALLTYPE targetAdd(IDropTarget *s){return InterlockedIncrement(&((Target*)s)->refs);}
static ULONG STDMETHODCALLTYPE targetRelease(IDropTarget *s){return InterlockedDecrement(&((Target*)s)->refs);}
static HRESULT STDMETHODCALLTYPE targetEnter(IDropTarget *s,IDataObject *data,DWORD keys,POINTL point,DWORD *effect){FORMATETC f={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};(void)s;(void)keys;(void)point;enters++;if(failEnter)return E_ACCESSDENIED;check(IDataObject_QueryGetData(data,&f)==S_OK,"target can query file data");*effect&=DROPEFFECT_COPY;return S_OK;}
static HRESULT STDMETHODCALLTYPE targetOver(IDropTarget *s,DWORD keys,POINTL point,DWORD *effect){(void)s;(void)keys;(void)point;overs++;*effect&=DROPEFFECT_COPY;return S_OK;}
static HRESULT STDMETHODCALLTYPE targetLeave(IDropTarget *s){(void)s;leaves++;return S_OK;}
static HRESULT STDMETHODCALLTYPE targetDrop(IDropTarget *s,IDataObject *data,DWORD keys,POINTL point,DWORD *effect){FORMATETC f={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM medium;IEnumFORMATETC *e,*copy;ULONG count;char path[MAX_PATH];(void)s;(void)keys;(void)point;drops++;
 check(IDataObject_GetData(data,&f,&medium)==S_OK,"drop receives owned medium");check(DragQueryFileA((HDROP)medium.hGlobal,0,path,MAX_PATH)>0&&!lstrcmpA(path,"C:\\TEST.TXT"),"drop payload path");ReleaseStgMedium(&medium);check(medium.tymed==TYMED_NULL,"released medium reset");
 check(IDataObject_EnumFormatEtc(data,DATADIR_GET,&e)==S_OK,"format enumeration");check(IEnumFORMATETC_Next(e,1,&f,&count)==S_OK&&count==1&&f.cfFormat==CF_HDROP,"format enumeration item");check(IEnumFORMATETC_Clone(e,&copy)==S_OK&&IEnumFORMATETC_Next(copy,1,&f,&count)==S_FALSE,"format enumerator clone position");IEnumFORMATETC_Release(copy);IEnumFORMATETC_Release(e);formatsSeen++;*effect=DROPEFFECT_COPY;return S_OK;
}
static IDropTargetVtbl targetTable={targetQuery,targetAdd,targetRelease,targetEnter,targetOver,targetLeave,targetDrop};
static Target testTarget={{&targetTable},1};
typedef struct Source{IDropSource iface;LONG refs;unsigned queries;} Source;
static HRESULT STDMETHODCALLTYPE sourceQuery(IDropSource *s,REFIID id,void **out){if(!out)return E_POINTER;*out=NULL;if(!guidEqual(id,&IID_IUnknown)&&!guidEqual(id,&IID_IDropSource))return E_NOINTERFACE;*out=s;IDropSource_AddRef(s);return S_OK;}
static ULONG STDMETHODCALLTYPE sourceAdd(IDropSource *s){return InterlockedIncrement(&((Source*)s)->refs);}
static ULONG STDMETHODCALLTYPE sourceRelease(IDropSource *s){return InterlockedDecrement(&((Source*)s)->refs);}
static HRESULT STDMETHODCALLTYPE sourceContinue(IDropSource *s,BOOL escape,DWORD keys){(void)keys;((Source*)s)->queries++;if(cancelDrag||escape)return DRAGDROP_S_CANCEL;return ((Source*)s)->queries>1?DRAGDROP_S_DROP:S_OK;}
static HRESULT STDMETHODCALLTYPE sourceFeedback(IDropSource *s,DWORD effect){(void)s;(void)effect;return failFeedback?E_ABORT:DRAGDROP_S_USEDEFAULTCURSORS;}
static IDropSourceVtbl sourceTable={sourceQuery,sourceAdd,sourceRelease,sourceContinue,sourceFeedback};
static Source testSource={{&sourceTable},1,0};
static DWORD WINAPI threadTest(void *unused){(void)unused;check(CoInitialize(NULL)==S_OK,"independent thread apartment");CoUninitialize();return 0;}
static LRESULT CALLBACK childWindow(HWND w,UINT msg,WPARAM wp,LPARAM lp){if(msg==WM_USER+1)return drops;if(msg==WM_CLOSE){DestroyWindow(w);return 0;}if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(w,msg,wp,lp);}
static void child(void){WNDCLASSA wc;HWND w;MSG msg;report=CreateFileA("C:\\OLECHILD.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);check(OleInitialize(NULL)==S_OK,"child OLE initialization");ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=childWindow;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="Z7OleChildWindow";check(RegisterClassA(&wc)!=0,"register child window class");w=CreateWindowA(wc.lpszClassName,"OLE child",WS_OVERLAPPEDWINDOW|WS_VISIBLE,70,70,320,230,NULL,NULL,wc.hInstance,NULL);check(w!=NULL&&RegisterDragDrop(w,&testTarget.iface)==S_OK,"register child drop target");ShowWindow(w,SW_SHOW);SetForegroundWindow(w);UpdateWindow(w);while(GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}check(drops==1,"cross-process drop delivered once");check(testTarget.refs==1,"child target released");OleUninitialize();check(TRUE,"ALL");CloseHandle(report);ExitProcess(0);}
static BOOL childArgument(void){const char *p=GetCommandLineA();while(*p){if(p[0]=='-'&&p[1]=='-'&&p[2]=='c'&&p[3]=='h'&&p[4]=='i'&&p[5]=='l'&&p[6]=='d')return TRUE;p++;}return FALSE;}
void entry(void){
 static const CLSID testClass={0x72bf97c1,0x8d52,0x469a,{0xa4,0x31,0x79,0x73,0x62,0x16,0xef,0x20}};
 static const char key[]="CLSID\\{72BF97C1-8D52-469A-A431-79736216EF20}";
 char dll[MAX_PATH],subkey[100];DWORD disposition,n,effect,threadId;HKEY reg;IUnknown *object,*second;void *memory;HANDLE thread;HWND window;HGLOBAL h;DROPFILES *files;DropData *data;POINT point;HRESULT hr;WNDCLASSA wc;
 if(childArgument())child();
 report=CreateFileA("C:\\OLETEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 check(CoInitialize(NULL)==S_OK&&CoInitialize(NULL)==S_FALSE,"balanced nested COM initialization");CoUninitialize();CoUninitialize();check(OleInitialize(NULL)==S_OK,"OLE initialization");
 thread=CreateThread(NULL,0,threadTest,NULL,0,&threadId);check(thread!=NULL&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"thread apartment finishes");CloseHandle(thread);
 memory=CoTaskMemAlloc(123);check(memory!=NULL,"task allocator");memset(memory,0x5a,123);CoTaskMemFree(memory);CoTaskMemFree(NULL);
 GetModuleFileNameA(NULL,dll,MAX_PATH);n=lstrlenA(dll);while(n&&dll[n-1]!='\\')n--;lstrcpyA(dll+n,"CLASS.DLL");lstrcpyA(subkey,key);lstrcatA(subkey,"\\InprocServer32");
 check(RegCreateKeyExA(HKEY_CLASSES_ROOT,subkey,0,NULL,0,KEY_WRITE,NULL,&reg,&disposition)==ERROR_SUCCESS,"register isolated test class");check(RegSetValueExA(reg,NULL,0,REG_SZ,(BYTE*)dll,lstrlenA(dll)+1)==ERROR_SUCCESS,"test class server path");RegCloseKey(reg);
 check(CoCreateInstance(&testClass,NULL,CLSCTX_INPROC_SERVER,&IID_IUnknown,(void**)&object)==S_OK&&object!=NULL,"create registered COM class");check(IUnknown_QueryInterface(object,&IID_IUnknown,(void**)&second)==S_OK&&second==object,"COM identity");IUnknown_Release(second);check(IUnknown_Release(object)==0,"COM object lifetime");RegDeleteKeyA(HKEY_CLASSES_ROOT,subkey);RegDeleteKeyA(HKEY_CLASSES_ROOT,key);
 object=(IUnknown*)1;check(CoCreateInstance(&testClass,NULL,CLSCTX_INPROC_SERVER,&IID_IUnknown,(void**)&object)==REGDB_E_CLASSNOTREG&&object==NULL,"absent class returns registration error");
 ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=DefWindowProcA;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="Z7OleTestWindow";check(RegisterClassA(&wc)!=0,"register drop window class");
 window=CreateWindowA(wc.lpszClassName,"OLE test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,30,30,300,220,NULL,NULL,GetModuleHandleA(NULL),NULL);check(window!=NULL,"create drop window");ShowWindow(window,SW_SHOW);SetActiveWindow(window);SetForegroundWindow(window);BringWindowToTop(window);UpdateWindow(window);
 check(RegisterDragDrop(window,&testTarget.iface)==S_OK,"register drop target");check(RegisterDragDrop(window,&testTarget.iface)==DRAGDROP_E_ALREADYREGISTERED,"duplicate target rejected");
 h=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,sizeof(DROPFILES)+14);files=GlobalLock(h);check(files!=NULL,"create file-drop payload");files->pFiles=sizeof(DROPFILES);lstrcpyA((char*)files+sizeof(DROPFILES),"C:\\TEST.TXT");GlobalUnlock(h);data=makeData(h);check(data!=NULL,"create file data object");
 point.x=60;point.y=80;ClientToScreen(window,&point);SetCursorPos(point.x,point.y);check(WindowFromPoint(point)==window,"drop window hit test");hr=DoDragDrop(&data->iface,&testSource.iface,DROPEFFECT_COPY|DROPEFFECT_MOVE,&effect);{char text[160];wsprintfA(text,"drag result %08lX effect %08lX enters %u overs %u drops %u",hr,effect,enters,overs,drops);check(TRUE,text);}
 check(hr==DRAGDROP_S_DROP&&effect==DROPEFFECT_COPY&&drops==1&&overs>0,"drag loop negotiates and delivers drop");
 cancelDrag=TRUE;testSource.queries=0;hr=DoDragDrop(&data->iface,&testSource.iface,DROPEFFECT_COPY,&effect);check(hr==DRAGDROP_S_CANCEL&&effect==0&&leaves>0,"drag cancellation leaves target");cancelDrag=FALSE;
 failEnter=TRUE;testSource.queries=0;hr=DoDragDrop(&data->iface,&testSource.iface,DROPEFFECT_COPY,&effect);check(hr==E_ACCESSDENIED&&effect==0,"target error propagated");failEnter=FALSE;
 failFeedback=TRUE;testSource.queries=0;hr=DoDragDrop(&data->iface,&testSource.iface,DROPEFFECT_COPY,&effect);check(hr==E_ABORT&&effect==0,"source error propagated");failFeedback=FALSE;
 SendMessageA(window,WM_DROPFILES,(WPARAM)h,0);check(drops==2&&formatsSeen==2,"legacy incoming file drop adapted");IDataObject_Release(&data->iface);
 check(RevokeDragDrop(window)==S_OK&&testTarget.refs==1,"revoke releases target");check(RevokeDragDrop(window)==DRAGDROP_E_NOTREGISTERED,"repeat revoke rejected");check(RegisterDragDrop(window,&testTarget.iface)==S_OK,"register after revoke");DestroyWindow(window);check(testTarget.refs==1&&testSource.refs==1,"window destruction releases ownership");
 {
  STARTUPINFOA si;PROCESS_INFORMATION pi;char executable[MAX_PATH],command[2*MAX_PATH];DWORD start,status;HWND other;RECT rect;HGLOBAL payload;DROPFILES *drop;DropData *transfer;
  GetModuleFileNameA(NULL,executable,MAX_PATH);wsprintfA(command,"\"%s\" --child",executable);ZeroMemory(&si,sizeof(si));ZeroMemory(&pi,sizeof(pi));si.cb=sizeof(si);
  check(CreateProcessA(executable,command,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"launch independent drop receiver");CloseHandle(pi.hThread);start=GetTickCount();other=NULL;while(GetTickCount()-start<10000){other=FindWindowA("Z7OleChildWindow",NULL);if(other&&GetPropA(other,DROP_PROPERTY))break;Sleep(20);}check(other!=NULL,"child receiver becomes available");
  SetForegroundWindow(other);GetWindowRect(other,&rect);point.x=rect.left+80;point.y=rect.top+100;SetCursorPos(point.x,point.y);
  payload=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,sizeof(DROPFILES)+14);drop=GlobalLock(payload);check(drop!=NULL,"allocate cross-process payload");drop->pFiles=sizeof(DROPFILES);lstrcpyA((char*)drop+sizeof(DROPFILES),"C:\\TEST.TXT");GlobalUnlock(payload);transfer=makeData(payload);GlobalFree(payload);check(transfer!=NULL,"build cross-process data object");testSource.queries=0;
  hr=DoDragDrop(&transfer->iface,&testSource.iface,DROPEFFECT_COPY,&effect);check(hr==DRAGDROP_S_DROP&&effect==DROPEFFECT_COPY,"cross-process drag reports copy");IDataObject_Release(&transfer->iface);check(SendMessageA(other,WM_USER+1,0,0)==1,"receiver confirms cross-process payload");SendMessageA(other,WM_CLOSE,0,0);check(WaitForSingleObject(pi.hProcess,10000)==WAIT_OBJECT_0&&GetExitCodeProcess(pi.hProcess,&status)&&status==0,"child receiver lifetime completes");CloseHandle(pi.hProcess);
 }
 OleUninitialize();check(TRUE,"ALL");CloseHandle(report);ExitProcess(0);
}
