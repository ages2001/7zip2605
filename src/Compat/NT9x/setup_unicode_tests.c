/* NT4 installation paths outside the ANSI code page, with actual shell links.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
static HANDLE report;static unsigned failures;
static void check(BOOL ok,const char *text){DWORD n;WriteFile(report,ok?"PASS ":"FAIL ",5,&n,NULL);WriteFile(report,text,lstrlenA(text),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);if(!ok)failures++;}
static DWORD run(const WCHAR *exe,const WCHAR *args){WCHAR command[3*MAX_PATH];STARTUPINFOW si;PROCESS_INFORMATION pi;DWORD result=0xffffffff;ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);wsprintfW(command,L"\"%s\" %s",exe,args);if(!CreateProcessW(exe,command,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi))return GetLastError();if(WaitForSingleObject(pi.hProcess,90000)==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&result);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return result;}
static BOOL target(const WCHAR *folder,const WCHAR *expected){HKEY key;WCHAR path[MAX_PATH],actual[MAX_PATH];DWORD bytes=sizeof(path),type;IShellLinkW *link=NULL;IPersistFile *persist=NULL;HRESULT hr;BOOL ok=FALSE;
 if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",0,KEY_QUERY_VALUE,&key))return FALSE;
 hr=RegQueryValueExW(key,folder,NULL,&type,(BYTE*)path,&bytes);RegCloseKey(key);if(hr||type!=REG_SZ||bytes<2||bytes>sizeof(path)||path[bytes/2-1]||lstrlenW(path)+23>=MAX_PATH)return FALSE;lstrcatW(path,L"\\7-Zip File Manager.lnk");
 hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);if(FAILED(hr))return FALSE;
 hr=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&persist);if(SUCCEEDED(hr))hr=IPersistFile_Load(persist,path,STGM_READ);if(SUCCEEDED(hr))hr=IShellLinkW_GetPath(link,actual,MAX_PATH,NULL,SLGP_RAWPATH);if(SUCCEEDED(hr))ok=!lstrcmpiW(actual,expected);if(persist)IPersistFile_Release(persist);IShellLinkW_Release(link);return ok;
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show){WCHAR setup[MAX_PATH],root[MAX_PATH],gui[MAX_PATH],uninstall[MAX_PATH],command[2*MAX_PATH],*p;DWORD start,version=GetVersion();
 report=CreateFileA("C:\\SETUNIC.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)return 2;
 if((version&0x80000000UL)||(version&255)<4){check(FALSE,"fixture requires NT4 or later");CloseHandle(report);return 2;}
 GetModuleFileNameW(NULL,setup,MAX_PATH);p=setup+lstrlenW(setup);while(p>setup&&p[-1]!='\\')p--;lstrcpyW(p,L"SETUP.EXE");wsprintfW(root,L"C:\\Z7U%05lX\x4e2d",GetTickCount()&0xfffff);
 check(GetFileAttributesW(root)==INVALID_FILE_ATTRIBUTES,"private Unicode destination is unused");wsprintfW(command,L"/S /D=%s",root);check(run(setup,command)==0,"install into Unicode path");lstrcpyW(gui,root);lstrcatW(gui,L"\\7zFM.exe");check(GetFileAttributesW(gui)!=INVALID_FILE_ATTRIBUTES,"installed Unicode GUI path exists");
 CoInitialize(NULL);check(target(L"Desktop",gui),"desktop shortcut retains exact UTF-16 target");check(target(L"Programs",gui),"Programs shortcut retains exact UTF-16 target");CoUninitialize();
 lstrcpyW(uninstall,root);lstrcatW(uninstall,L"\\Uninst.exe");check(run(uninstall,L"/S")==0,"start uninstaller from Unicode path");start=GetTickCount();while(GetTickCount()-start<30000&&GetFileAttributesW(root)!=INVALID_FILE_ATTRIBUTES)Sleep(100);check(GetFileAttributesW(root)==INVALID_FILE_ATTRIBUTES,"uninstaller removes empty Unicode install directory");check(failures==0,"ALL");CloseHandle(report);return failures?1:0;
}
