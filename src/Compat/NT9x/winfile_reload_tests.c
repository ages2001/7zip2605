/* Exercise the documented add-on load/unload lifecycle in one process. */
#include <windows.h>
void *memset(void *p,int v,unsigned n){unsigned char *q=p;while(n--)*q++=(unsigned char)v;return p;}
typedef struct {DWORD size;CHAR name[40];HMENU menu;UINT delta;} LoadA;
typedef LONG (WINAPI *Extension)(HWND,WPARAM,LPARAM);
static DWORD countSlots(void){DWORD ids[1024],n=0,id;while(n<1024&&(id=TlsAlloc())!=TLS_OUT_OF_INDEXES)ids[n++]=id;id=n;while(n)TlsFree(ids[--n]);return id;}
void entry(void){
 HWND window;HMODULE module;Extension extension;LoadA data;HANDLE log;DWORD n,before,after;unsigned i;char line[160];BOOL ok=TRUE;
 log=CreateFileA("C:\\WFRELOAD.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 window=CreateWindowA("STATIC","Extension reload fixture",WS_OVERLAPPEDWINDOW,0,0,160,120,0,0,GetModuleHandleA(0),0);
 before=countSlots();
 for(i=0;window&&i<200;i++){
  module=LoadLibraryA("A:\\7zWF.dll");if(!module){ok=FALSE;break;}
  extension=(Extension)GetProcAddress(module,"FMExtensionProc");memset(&data,0,sizeof(data));data.size=sizeof(data);
  if(!extension||!extension(window,100,(LPARAM)&data)){ok=FALSE;FreeLibrary(module);break;}
  if(!extension(window,101,0))ok=FALSE;
  if(data.menu)DestroyMenu(data.menu);FreeLibrary(module);if(!ok)break;
 }
 after=countSlots();ok=ok&&window&&i==200&&before==after;
 wsprintfA(line,"%s: cycles=%u TLS available before=%lu after=%lu\r\n",ok?"PASS":"FAIL",i,before,after);WriteFile(log,line,lstrlenA(line),&n,0);FlushFileBuffers(log);CloseHandle(log);if(window)DestroyWindow(window);ExitProcess(ok?0:1);
}
