/* Actual install/uninstall fixture. Uses the packaged executables and the
   guest shell, and keeps a user-owned file to detect overbroad cleanup.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>
#include <string.h>
#include "setup_shortcuts.h"
static HANDLE report;static unsigned failures;
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);}
static void check(BOOL ok,const char *s){line(ok?"PASS":"FAIL");line(s);if(!ok)failures++;}
static DWORD run(const char *exe,const char *args){STARTUPINFOA si;PROCESS_INFORMATION pi;char cmd[3*MAX_PATH];DWORD code=0xffffffff;ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);wsprintfA(cmd,"\"%s\" %s",exe,args);line(cmd);if(!CreateProcessA(exe,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)){line("CreateProcess failed");return GetLastError();}if(WaitForSingleObject(pi.hProcess,90000)==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);wsprintfA(cmd,"Exit: %lu",code);line(cmd);return code;}
static HDDEDATA CALLBACK callback(UINT a,UINT b,HCONV c,HSZ d,HSZ e,HDDEDATA f,ULONG_PTR g,ULONG_PTR h){return 0;}
static void dumpGroup(void){DWORD id=0,n;HSZ service,item;HCONV conversation;HDDEDATA data;const char *p;if(DdeInitializeA(&id,callback,APPCMD_CLIENTONLY,0))return;service=DdeCreateStringHandleA(id,"PROGMAN",CP_WINANSI);item=DdeCreateStringHandleA(id,"7-Zip 26.05",CP_WINANSI);conversation=DdeConnect(id,service,service,NULL);if(conversation){data=DdeClientTransaction(NULL,0,conversation,item,CF_TEXT,XTYP_REQUEST,10000,NULL);if(data){p=(const char*)DdeAccessData(data,&n);if(p&&n&&memchr(p,0,n))line(p);if(p)DdeUnaccessData(data);DdeFreeDataHandle(data);}DdeDisconnect(conversation);}DdeFreeStringHandle(id,item);DdeFreeStringHandle(id,service);DdeUninitialize(id);}
static BOOL isNt3(void){DWORD v=GetVersion();return !(v&0x80000000UL)&&(v&255)==3;}
static BOOL groupText(char *out,DWORD capacity,const char *command){DWORD id=0,n;HSZ service,item;HCONV conversation;HDDEDATA data;const char *p;BOOL ok=FALSE;out[0]=0;if(DdeInitializeA(&id,callback,APPCMD_CLIENTONLY,0))return FALSE;service=DdeCreateStringHandleA(id,"PROGMAN",CP_WINANSI);item=DdeCreateStringHandleA(id,"7-Zip 26.05",CP_WINANSI);conversation=DdeConnect(id,service,service,NULL);if(conversation){if(command)ok=DdeClientTransaction((LPBYTE)command,lstrlenA(command)+1,conversation,0,0,XTYP_EXECUTE,10000,NULL)!=0;else{data=DdeClientTransaction(NULL,0,conversation,item,CF_TEXT,XTYP_REQUEST,10000,NULL);if(data){p=(const char*)DdeAccessData(data,&n);if(p&&n&&memchr(p,0,n)&&lstrlenA(p)<capacity){lstrcpyA(out,p);ok=TRUE;}if(p)DdeUnaccessData(data);DdeFreeDataHandle(data);}}DdeDisconnect(conversation);}DdeFreeStringHandle(id,item);DdeFreeStringHandle(id,service);DdeUninitialize(id);return ok;}
static void checkItems(BOOL installed){char data[4096];const char *p;BOOL got;if(!isNt3())return;got=groupText(data,sizeof(data),NULL);line(data);if(installed){check(got,"Program Manager group can be queried");p=strstr(data,"\"7-Zip File Manager\",\"C:\\7-Zip\\7zFM.exe\",");check(p&&!strstr(p+1,"\"7-Zip File Manager\","),"exactly one File Manager item targets installed GUI");p=strstr(data,"\"Uninstall 7-Zip\",\"C:\\7-Zip\\Uninst.exe\",");check(p&&!strstr(p+1,"\"Uninstall 7-Zip\","),"exactly one interactive Uninstall item targets installed uninstaller without /S");}else check(!strstr(data,"\"7-Zip File Manager\",")&&!strstr(data,"\"Uninstall 7-Zip\","),"uninstall removes both owned Program Manager items");}
typedef struct GuiSearch { DWORD process; HWND window; } GuiSearch;
static BOOL CALLBACK findGui(HWND window,LPARAM parameter){GuiSearch *s=(GuiSearch*)parameter;DWORD process;char name[64];GetWindowThreadProcessId(window,&process);GetClassNameA(window,name,sizeof(name));if(process==s->process&&!lstrcmpA(name,"7-Zip::FM")){s->window=window;return FALSE;}return TRUE;}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show)
{
 char setup[MAX_PATH],directory[MAX_PATH],exe[MAX_PATH],keep[MAX_PATH],uninstall[MAX_PATH],*p;
 HANDLE file;DWORD n,begin;HWND gui;
 report=CreateFileA("C:\\SETTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)return 2;
 GetModuleFileNameA(NULL,setup,MAX_PATH);p=setup+lstrlenA(setup);while(p>setup&&p[-1]!='\\')--p;lstrcpyA(p,"SETUP.EXE");
 check(Z7SetupDefaultPath(directory,MAX_PATH)==0,"OS-specific default install directory");line(directory);
 if(!(GetVersion()&0x80000000UL)&&(GetVersion()&255)==3)dumpGroup();
 check(Z7SetupRemoveShortcuts("C:\\Z7LINKT\\7ZFM.EXE")==0,"remove only the earlier shortcut-test target");
 check(run(setup,"/S")==0,"silent install returns success");checkItems(TRUE);
 lstrcpyA(exe,directory);lstrcatA(exe,"\\7z.exe");check(GetFileAttributesA(exe)!=INVALID_FILE_ATTRIBUTES,"installed CLI exists");check(run(exe,"i")==0,"installed CLI loads all codecs");
 lstrcpyA(exe,directory);lstrcatA(exe,"\\7zFM.exe");check(GetFileAttributesA(exe)!=INVALID_FILE_ATTRIBUTES,"installed GUI exists");
 {STARTUPINFOA si;PROCESS_INFORMATION pi;ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);
 if(CreateProcessA(exe,NULL,NULL,NULL,FALSE,0,NULL,directory,&si,&pi)){
  begin=GetTickCount();gui=NULL;while(GetTickCount()-begin<60000){{GuiSearch search;search.process=pi.dwProcessId;search.window=NULL;EnumWindows(findGui,(LPARAM)&search);gui=search.window;}if(gui)break;Sleep(50);}check(gui!=NULL,"installed GUI opens");if(gui)PostMessageA(gui,WM_CLOSE,0,0);check(WaitForSingleObject(pi.hProcess,60000)==WAIT_OBJECT_0,"installed GUI exits normally");CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 }else check(FALSE,"installed GUI process");}
 {char alternate[MAX_PATH],other[MAX_PATH];wsprintfA(alternate,"C:\\Z7L%05lX",GetTickCount()&0xfffff);check(CreateDirectoryA(alternate,NULL),"private shortcut ownership probe");wsprintfA(other,"%s\\7ZFM.EXE",alternate);check(CopyFileA(exe,other,TRUE),"different shortcut target exists");check(Z7SetupCreateShortcuts(other,alternate)==ERROR_ALREADY_EXISTS,"installation refuses an unrelated shortcut target");check(Z7SetupRemoveShortcuts(other)==ERROR_SUCCESS,"removal preserves an unrelated shortcut target");check(Z7SetupCreateShortcuts(exe,directory)==ERROR_SUCCESS,"original shortcut remains owned by installation");DeleteFileA(other);RemoveDirectoryA(alternate);}
 if(isNt3()){char data[4096];check(groupText(data,sizeof(data),"[ShowGroup(\"7-Zip 26.05\",1)][DeleteItem(\"Uninstall 7-Zip\")]"),"simulate an older installation with only File Manager shortcut");}
 check(run(setup,"/S")==0,"reinstall upgrades older File Manager-only group");checkItems(TRUE);
 check(run(setup,"/S")==0,"reinstall updates both items without duplicates");checkItems(TRUE);
 lstrcpyA(keep,directory);wsprintfA(keep+lstrlenA(keep),"\\K%05lX.TXT",GetTickCount()&0xfffff);file=CreateFileA(keep,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);check(file!=INVALID_HANDLE_VALUE,"create user-owned file");if(file!=INVALID_HANDLE_VALUE){WriteFile(file,"USER DATA",9,&n,NULL);CloseHandle(file);}
 lstrcpyA(uninstall,directory);lstrcatA(uninstall,"\\Uninst.exe");
 {char oldTmp[MAX_PATH]={0},oldTemp[MAX_PATH]={0},missing[MAX_PATH],selected[MAX_PATH];DWORD tmpLen,tempLen;BOOL selectedMissing;
  tmpLen=GetEnvironmentVariableA("TMP",oldTmp,MAX_PATH);tempLen=GetEnvironmentVariableA("TEMP",oldTemp,MAX_PATH);
  check(tmpLen<MAX_PATH&&tempLen<MAX_PATH,"temporary environment fits fixture buffers");
  wsprintfA(missing,"C:\\Z7U%05lX\\TEMP",GetTickCount()&0xfffff);
  check(GetFileAttributesA(missing)==INVALID_FILE_ATTRIBUTES,"nested temporary directory is absent before uninstall");
  check(SetEnvironmentVariableA("TMP",missing)&&SetEnvironmentVariableA("TEMP",missing),"set missing temporary directory for uninstaller");
  check(GetTempPathA(MAX_PATH,selected)>0,"query native temporary-directory selection");
  {unsigned len=lstrlenA(selected);if(len>3&&selected[len-1]=='\\')selected[len-1]=0;}
  line("Native temporary directory:");line(selected);selectedMissing=!lstrcmpiA(selected,missing);
  check(run(uninstall,"/S")==0,"uninstaller starts successfully");
  SetEnvironmentVariableA("TMP",tmpLen?oldTmp:NULL);SetEnvironmentVariableA("TEMP",tempLen?oldTemp:NULL);
  {DWORD attrs=GetFileAttributesA(selectedMissing?missing:selected);
   check(attrs!=INVALID_FILE_ATTRIBUTES&&(attrs&FILE_ATTRIBUTE_DIRECTORY),selectedMissing?"uninstaller creates missing temporary directory":"native OS fallback temporary directory remains usable");
  }
 }
 begin=GetTickCount();
 while(GetTickCount()-begin<30000){
  HKEY pending;LONG status=RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\7-Zip",0,KEY_READ,&pending);
  if(status==ERROR_SUCCESS)RegCloseKey(pending);
  if(GetFileAttributesA(exe)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesA(uninstall)==INVALID_FILE_ATTRIBUTES&&status==ERROR_FILE_NOT_FOUND)break;
  Sleep(100);
 }
 check(GetFileAttributesA(exe)==INVALID_FILE_ATTRIBUTES,"uninstaller removes GUI");check(GetFileAttributesA(uninstall)==INVALID_FILE_ATTRIBUTES,"uninstaller removes original executable");
 file=CreateFileA(keep,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);{char data[16];n=0;if(file!=INVALID_HANDLE_VALUE){ReadFile(file,data,16,&n,NULL);CloseHandle(file);}check(n==9&&!memcmp(data,"USER DATA",9),"uninstall preserves user-owned file");}
 if(!(GetVersion()&0x80000000UL)&&(GetVersion()&255)==3)dumpGroup();
 checkItems(FALSE);
 check(failures==0,"ALL");CloseHandle(report);return failures?1:0;
}
