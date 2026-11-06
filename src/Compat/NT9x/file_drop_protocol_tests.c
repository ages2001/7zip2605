/* Cross-process copy/move transport and source-ownership regression tests. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
void *memset(void *p,int c,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=(BYTE)c;return p;}
void *memcpy(void *d,const void *s,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)d)[i]=((const BYTE*)s)[i];return d;}
#include "legacy_ole.c"
static HANDLE logFile;
static char base[MAX_PATH];
static const char payload[]="7-Zip file-drop transport: exact bytes\r\n";
static unsigned drops;
static BOOL cancelSource;
static void record(BOOL ok,const char *text){DWORD n;const char *prefix=ok?"PASS ":"FAIL ";WriteFile(logFile,prefix,lstrlenA(prefix),&n,0);WriteFile(logFile,text,lstrlenA(text),&n,0);WriteFile(logFile,"\r\n",2,&n,0);FlushFileBuffers(logFile);if(!ok)ExitProcess(2);}
static BOOL exact(const char *path){char b[sizeof(payload)];DWORD n;HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);BOOL ok;if(f==INVALID_HANDLE_VALUE)return FALSE;ZeroMemory(b,sizeof(b));ok=GetFileSize(f,0)==sizeof(payload)-1&&ReadFile(f,b,sizeof(b),&n,0)&&n==sizeof(payload)-1&&!lstrcmpA(b,payload);CloseHandle(f);return ok;}
typedef struct { IDropTarget iface; LONG refs; DWORD allowed; } Target;
static HRESULT STDMETHODCALLTYPE targetQuery(IDropTarget *s,REFIID id,void **out){if(!out)return E_POINTER;*out=NULL;if(!guidEqual(id,&IID_IUnknown)&&!guidEqual(id,&IID_IDropTarget))return E_NOINTERFACE;*out=s;IDropTarget_AddRef(s);return S_OK;}
static ULONG STDMETHODCALLTYPE targetAdd(IDropTarget *s){return InterlockedIncrement(&((Target*)s)->refs);}
static ULONG STDMETHODCALLTYPE targetRelease(IDropTarget *s){return InterlockedDecrement(&((Target*)s)->refs);}
static HRESULT STDMETHODCALLTYPE targetEnter(IDropTarget *s,IDataObject *d,DWORD k,POINTL p,DWORD *effect){FORMATETC f={CF_HDROP,0,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};(void)p;((Target*)s)->allowed=SUCCEEDED(IDataObject_QueryGetData(d,&f))?*effect:0;*effect=fileDropEffect(((Target*)s)->allowed,k);return S_OK;}
static HRESULT STDMETHODCALLTYPE targetOver(IDropTarget *s,DWORD k,POINTL p,DWORD *effect){(void)p;*effect=fileDropEffect(((Target*)s)->allowed,k);return S_OK;}
static HRESULT STDMETHODCALLTYPE targetLeave(IDropTarget *s){((Target*)s)->allowed=0;return S_OK;}
static HRESULT STDMETHODCALLTYPE targetDrop(IDropTarget *s,IDataObject *d,DWORD k,POINTL p,DWORD *effect){
 FORMATETC f={CF_HDROP,0,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM medium;DWORD selected=fileDropEffect(((Target*)s)->allowed,k);UINT count,i;char src[MAX_PATH],dest[MAX_PATH],*name;HRESULT hr;(void)p;*effect=0;
 if(!selected)return S_OK;hr=IDataObject_GetData(d,&f,&medium);if(FAILED(hr))return hr;
 record(medium.tymed==TYMED_HGLOBAL,"child receives file medium");count=DragQueryFileA((HDROP)medium.hGlobal,0xffffffff,0,0);record(count==2,"child receives two selected files");
 for(i=0;i<count;i++){
  record(DragQueryFileA((HDROP)medium.hGlobal,i,src,sizeof(src))!=0&&exact(src),"child sees exact source bytes");name=src+lstrlenA(src);while(name>src&&name[-1]!='\\')--name;wsprintfA(dest,"%s\\OUT\\%s",base,name);
  record(selected==DROPEFFECT_MOVE?MoveFileA(src,dest):CopyFileA(src,dest,TRUE),selected==DROPEFFECT_MOVE?"child physically moves source":"child physically copies source");record(exact(dest),"child verifies destination bytes");
 }
 ReleaseStgMedium(&medium);{char detail[100];HRESULT set=fileDropPerformed(d,selected);wsprintfA(detail,"child performed-effect SetData=%08lX",set);record(SUCCEEDED(set),detail);}*effect=selected==DROPEFFECT_MOVE?DROPEFFECT_NONE:DROPEFFECT_COPY;++drops;return S_OK;
}
static IDropTargetVtbl targetTable={targetQuery,targetAdd,targetRelease,targetEnter,targetOver,targetLeave,targetDrop};
static Target target={{&targetTable},1,0};
typedef struct { IDropSource iface; LONG refs; unsigned queries; } Source;
static HRESULT STDMETHODCALLTYPE sourceQuery(IDropSource *s,REFIID id,void **out){if(!out)return E_POINTER;*out=NULL;if(!guidEqual(id,&IID_IUnknown)&&!guidEqual(id,&IID_IDropSource))return E_NOINTERFACE;*out=s;IDropSource_AddRef(s);return S_OK;}
static ULONG STDMETHODCALLTYPE sourceAdd(IDropSource *s){return InterlockedIncrement(&((Source*)s)->refs);}
static ULONG STDMETHODCALLTYPE sourceRelease(IDropSource *s){return InterlockedDecrement(&((Source*)s)->refs);}
static HRESULT STDMETHODCALLTYPE sourceContinue(IDropSource *s,BOOL escape,DWORD k){(void)k;if(cancelSource||escape)return DRAGDROP_S_CANCEL;++((Source*)s)->queries;return !(k&MK_LBUTTON)?DRAGDROP_S_DROP:S_OK;}
static HRESULT STDMETHODCALLTYPE sourceFeedback(IDropSource *s,DWORD effect){(void)s;(void)effect;return DRAGDROP_S_USEDEFAULTCURSORS;}
static HWND sourceWindow,receiverWindow;
static void pumpInput(void){MSG message;while(PeekMessageA(&message,0,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}}
static DWORD WINAPI dragMouse(void *unused){POINT point={100,100};(void)unused;ClientToScreen(receiverWindow,&point);Sleep(250);SetCursorPos(point.x,point.y);mouse_event(MOUSEEVENTF_MOVE,1,0,0,0);Sleep(600);mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);return 0;}
static IDropSourceVtbl sourceTable={sourceQuery,sourceAdd,sourceRelease,sourceContinue,sourceFeedback};
static Source source={{&sourceTable},1,0};
static void malformedPackets(HWND receiver){
 struct {Z7FileDropPacket packet;Z7DropFilesHeader header;BYTE names[64];} wire;
 COPYDATASTRUCT copy;unsigned test;const char *description;
 if(fileDropCapabilities(receiver)!=3)return;
 for(test=0;test<9;test++){
  ZeroMemory(&wire,sizeof(wire));wire.packet.version=1;wire.packet.effects=3;wire.packet.dataSize=sizeof(wire)-sizeof(wire.packet);wire.header.pFiles=sizeof(wire.header);lstrcpyA((char*)wire.names,"C:\\ONE.TXT");lstrcpyA((char*)wire.names+11,"C:\\TWO.TXT");
  copy.dwData=Z7_DROP_PROTOCOL_TAG;copy.cbData=sizeof(wire);copy.lpData=&wire;
  switch(test){
   case 0:wire.packet.version=2;description="reject unsupported wire version";break;
   case 1:wire.packet.dataSize++;description="reject inconsistent payload length";break;
   case 2:wire.header.pFiles=0;description="reject file list inside header";break;
   case 3:wire.header.pFiles=0xffffffff;description="reject file list beyond packet";break;
   case 4:memset(wire.names,'X',sizeof(wire.names));description="reject unterminated ANSI list";break;
   case 5:wire.header.fWide=TRUE;wire.header.pFiles++;description="reject unaligned Unicode list";break;
   case 6:wire.header.fWide=TRUE;memset(wire.names,'X',sizeof(wire.names));description="reject unterminated Unicode list";break;
   case 7:wire.names[0]=0;description="reject empty file list";break;
   default:copy.cbData=sizeof(wire.packet)-1;description="reject truncated wire header";break;
  }
  record(SendMessageA(receiver,WM_COPYDATA,(WPARAM)sourceWindow,(LPARAM)&copy)==0,description);
  record(SendMessageA(receiver,WM_USER+57,0,0)==0,"malformed packet never performs a drop");
 }
}
static LRESULT CALLBACK childWindow(HWND w,UINT msg,WPARAM wp,LPARAM lp){if(msg==WM_USER+57)return drops;if(msg==WM_CLOSE){record(RevokeDragDrop(w)==S_OK,"child revokes target");RemovePropA(w,Z7_DROP_PROTOCOL_PROPERTY);DestroyWindow(w);return 0;}if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(w,msg,wp,lp);}
static void child(void){WNDCLASSA wc;HWND w;MSG msg;char path[MAX_PATH];wsprintfA(path,"%s\\CHILD.TXT",base);logFile=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_NEW,0,0);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);record(OleInitialize(0)==S_OK,"child initializes OLE");ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=childWindow;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="Z7FileTransport57";record(RegisterClassA(&wc)!=0,"child class");w=CreateWindowA(wc.lpszClassName,base,WS_OVERLAPPEDWINDOW|WS_VISIBLE,40,40,440,300,0,0,wc.hInstance,0);record(w&&RegisterDragDrop(w,&target.iface)==S_OK,"child registers target");record(SetPropA(w,Z7_DROP_PROTOCOL_PROPERTY,(HANDLE)3),"child advertises synchronous copy/move");ShowWindow(w,SW_SHOW);SetForegroundWindow(w);UpdateWindow(w);while(GetMessageA(&msg,0,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}record(target.refs==1,"target released at window destruction");OleUninitialize();record(TRUE,"ALL CHILD TESTS");CloseHandle(logFile);ExitProcess(0);}
static DropData *files(unsigned test)
{
 HGLOBAL global;
 Z7DropFilesHeader *header;
 char *q,path[MAX_PATH];
 unsigned i;
 DWORD n;
 HANDLE f;
 DropData *data;
 global=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,1024);
 header=GlobalLock(global);record(header!=0,"allocate drop list");
 header->pFiles=sizeof(*header);header->fWide=test==1;
 q=(char *)(header+1);
 for(i=0;i<2;i++){
  wsprintfA(path,"%s\\T%uF%u.TXT",base,test,i);
  f=CreateFileA(path,GENERIC_WRITE,0,0,CREATE_NEW,0,0);
  record(f!=INVALID_HANDLE_VALUE&&WriteFile(f,payload,sizeof(payload)-1,&n,0)&&n==sizeof(payload)-1,"create isolated source");
  CloseHandle(f);
  if(header->fWide){
   n=MultiByteToWideChar(CP_ACP,0,path,-1,(WCHAR*)q,MAX_PATH);
   record(n!=0,"build Unicode file list");q+=n*sizeof(WCHAR);
  }else{lstrcpyA(q,path);q+=lstrlenA(q)+1;}
 }
 GlobalUnlock(global);data=makeData(global);GlobalFree(global);
 record(data!=0,"create data object");return data;
}
void entry(void){
 const char *cmd=GetCommandLineA(),*p;char executable[MAX_PATH],command[3*MAX_PATH],path[MAX_PATH],dest[MAX_PATH];DWORD start,code,effect,i,test;STARTUPINFOA si;PROCESS_INFORMATION pi;HWND receiver=0;POINT point;HRESULT hr;DropData *data;
 for(p=cmd;*p;p++)if(p[0]==' '&&p[1]=='-'&&p[2]=='-'&&p[3]=='c'&&p[4]=='h'&&p[5]=='i'&&p[6]=='l'&&p[7]=='d'&&p[8]==' '){if(lstrlenA(p+9)>=MAX_PATH-32)ExitProcess(2);lstrcpyA(base,p+9);child();}
 logFile=CreateFileA("C:\\DROP57.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 for(i=0;i<1024;i++){wsprintfA(base,"C:\\D57%05lX",(GetTickCount()+i)&0xfffff);if(CreateDirectoryA(base,0))break;}record(i<1024,"create private fixture directory");record(TRUE,base);wsprintfA(path,"%s\\OUT",base);record(CreateDirectoryA(path,0),"create destination");
 record(OleInitialize(0)==S_OK,"source initializes OLE");sourceWindow=CreateWindowA("STATIC","7-Zip transport test source",WS_OVERLAPPEDWINDOW|WS_VISIBLE,8,350,240,110,0,0,GetModuleHandleA(0),0);record(sourceWindow!=0,"create source window");GetModuleFileNameA(0,executable,sizeof(executable));wsprintfA(command,"\"%s\" --child %s",executable,base);ZeroMemory(&si,sizeof(si));ZeroMemory(&pi,sizeof(pi));si.cb=sizeof(si);record(CreateProcessA(executable,command,0,0,FALSE,0,0,0,&si,&pi),"launch independent receiver");CloseHandle(pi.hThread);
 start=GetTickCount();while(GetTickCount()-start<15000){receiver=FindWindowA("Z7FileTransport57",base);if(receiver)break;Sleep(20);}record(receiver!=0,"receiver ready");WaitForInputIdle(pi.hProcess,10000);malformedPackets(receiver);
 for(test=0;test<5;test++){
  HANDLE driver;DWORD threadId;char detail[150];data=files(test);source.queries=0;cancelSource=test==4;SetForegroundWindow(sourceWindow);BringWindowToTop(sourceWindow);point.x=70;point.y=30;ClientToScreen(sourceWindow,&point);SetCursorPos(point.x,point.y);receiverWindow=receiver;
  wsprintfA(detail,"receiver advertised effects=%08lX",fileDropCapabilities(receiver));record(TRUE,detail);
  if(test==1)keybd_event(VK_CONTROL,0,0,0);if(test==2||test==3)keybd_event(VK_SHIFT,0,0,0);if(test==3)keybd_event(VK_CONTROL,0,0,0);Sleep(80);
  mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);Sleep(100);pumpInput();driver=CreateThread(0,0,dragMouse,0,0,&threadId);record(driver!=0,"start mouse driver");
  hr=DoDragDrop(&data->iface,&source.iface,DROPEFFECT_COPY|DROPEFFECT_MOVE,&effect);
  record(WaitForSingleObject(driver,5000)==WAIT_OBJECT_0,"mouse driver finishes");CloseHandle(driver);wsprintfA(detail,"test=%u result=%08lX effect=%08lX performed=%08lX queries=%u",test,hr,effect,data->performed,source.queries);record(TRUE,detail);
  keybd_event(VK_CONTROL,0,KEYEVENTF_KEYUP,0);keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);Sleep(80);
  record(test==3?(hr==DRAGDROP_S_DROP||hr==DRAGDROP_S_CANCEL):hr==(test==4?DRAGDROP_S_CANCEL:DRAGDROP_S_DROP),"correct success/cancel result");record(effect==(test<2?DROPEFFECT_COPY:DROPEFFECT_NONE),"no second source deletion requested");record(data->performed==(test<2?DROPEFFECT_COPY:test==2?DROPEFFECT_MOVE:0),"performed effect crosses process boundary");
  for(i=0;i<2;i++){wsprintfA(path,"%s\\T%uF%u.TXT",base,test,i);wsprintfA(dest,"%s\\OUT\\T%uF%u.TXT",base,test,i);record(test==2?GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES:exact(path),"source ownership and bytes");record(test>=3?GetFileAttributesA(dest)==INVALID_FILE_ATTRIBUTES:exact(dest),"destination ownership and bytes");}
  IDataObject_Release(&data->iface);record(source.refs==1,"source lifetime balanced");
 }
 record(SendMessageA(receiver,WM_USER+57,0,0)==3,"cancelled and unsupported-link drags do not invoke Drop");SendMessageA(receiver,WM_CLOSE,0,0);record(WaitForSingleObject(pi.hProcess,10000)==WAIT_OBJECT_0&&GetExitCodeProcess(pi.hProcess,&code)&&!code,"receiver exits cleanly");CloseHandle(pi.hProcess);DestroyWindow(sourceWindow);OleUninitialize();record(TRUE,"ALL TRANSPORT TESTS");CloseHandle(logFile);ExitProcess(0);
}
