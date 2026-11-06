/* Exercise the installer Unicode bridge against the guest's real ANSI APIs.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
static HANDLE logFile;static unsigned failures;
static void check(BOOL ok,const char *what){DWORD n;const char *prefix=ok?"PASS ":"FAIL ";WriteFile(logFile,prefix,5,&n,NULL);WriteFile(logFile,what,lstrlenA(what),&n,NULL);WriteFile(logFile,"\r\n",2,&n,NULL);FlushFileBuffers(logFile);if(!ok)failures++;}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show)
{
  WCHAR dir[MAX_PATH],file[MAX_PATH],next[MAX_PATH],module[MAX_PATH],command[2*MAX_PATH],buffer[128];
  char ansiPath[MAX_PATH],raw[128];DWORD n,size,type,exitCode;HKEY key;HANDLE f;HWND parent,edit;STARTUPINFOW si;PROCESS_INFORMATION pi;
  const WCHAR multi[]={L'o',L'n',L'e',0,L't',L'w',L'o',0,0};
  if(strstr(GetCommandLineA(),"--child"))return 37;
  logFile=CreateFileA("C:\\SETAPI.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(logFile==INVALID_HANDLE_VALUE)return 2;
  wsprintfA(ansiPath,"C:\\Z7S%05lX",GetTickCount()&0xfffff);MultiByteToWideChar(CP_ACP,0,ansiPath,-1,dir,MAX_PATH);
  check(CreateDirectoryW(dir,NULL),"CreateDirectoryW");
  lstrcpyW(file,dir);lstrcatW(file,L"\\DATA.TXT");lstrcpyW(next,dir);lstrcatW(next,L"\\NEXT.TXT");
  f=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);check(f!=INVALID_HANDLE_VALUE,"CreateFileW");if(f!=INVALID_HANDLE_VALUE){WriteFile(f,"exact bytes",11,&n,NULL);CloseHandle(f);}
  check(CopyFileW(file,next,TRUE)&&GetFileAttributesW(next)!=INVALID_FILE_ATTRIBUTES,"CopyFileW and attributes");
  check(DeleteFileW(next)&&MoveFileExW(file,next,0),"DeleteFileW and MoveFileExW");
  WideCharToMultiByte(CP_ACP,0,next,-1,ansiPath,MAX_PATH,NULL,NULL);f=CreateFileA(ansiPath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);ZeroMemory(raw,sizeof(raw));n=0;if(f!=INVALID_HANDLE_VALUE){ReadFile(f,raw,sizeof(raw),&n,NULL);CloseHandle(f);}check(n==11&&!memcmp(raw,"exact bytes",11),"native ANSI reader sees exact file bytes");
  check(GetFullPathNameW(next,128,buffer,NULL)>0&&!lstrcmpiW(buffer,next),"GetFullPathNameW");
  check(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\7-Zip\\SetupApiFixture",0,NULL,0,KEY_ALL_ACCESS,NULL,&key,NULL)==ERROR_SUCCESS,"RegCreateKeyExW");
  check(RegSetValueExW(key,L"Multi",0,REG_MULTI_SZ,(const BYTE*)multi,sizeof(multi))==ERROR_SUCCESS,"write Unicode MULTI_SZ");
  size=sizeof(raw);type=0;check(RegQueryValueExA(key,"Multi",NULL,&type,(BYTE*)raw,&size)==ERROR_SUCCESS&&type==REG_MULTI_SZ&&size==9&&!memcmp(raw,"one\0two\0",9),"native ANSI MULTI_SZ bytes");
  size=0;check(RegQueryValueExW(key,L"Multi",NULL,&type,NULL,&size)==ERROR_SUCCESS&&size==sizeof(multi),"query Unicode byte size");
  size=2;buffer[0]=0x1234;check(RegQueryValueExW(key,L"Multi",NULL,&type,(BYTE*)buffer,&size)==ERROR_MORE_DATA&&size==sizeof(multi)&&buffer[0]==0x1234,"small registry buffer preserves caller data");
  size=sizeof(buffer);check(RegQueryValueExW(key,L"Multi",NULL,&type,(BYTE*)buffer,&size)==ERROR_SUCCESS&&size==sizeof(multi)&&!memcmp(buffer,multi,size),"read Unicode MULTI_SZ");
  n=0x12345678;size=sizeof(n);check(RegSetValueExW(key,L"Number",0,REG_DWORD,(BYTE*)&n,sizeof(n))==ERROR_SUCCESS&&RegQueryValueExW(key,L"Number",NULL,&type,(BYTE*)&exitCode,&size)==ERROR_SUCCESS&&exitCode==n&&type==REG_DWORD,"binary registry data is unchanged");
  check(RegDeleteValueW(key,L"Multi")==ERROR_SUCCESS,"RegDeleteValueW");RegCloseKey(key);check(RegDeleteKeyW(HKEY_CURRENT_USER,L"Software\\7-Zip\\SetupApiFixture")==ERROR_SUCCESS,"RegDeleteKeyW");
  parent=CreateWindowA("STATIC","Fixture",WS_OVERLAPPED,0,0,200,80,NULL,NULL,instance,NULL);
  edit=CreateWindowA("EDIT","",WS_CHILD,0,0,100,20,parent,(HMENU)100,instance,NULL);
  check(parent&&edit&&SetDlgItemTextW(parent,100,L"Setup path")&&GetDlgItemTextW(parent,100,buffer,128)==10&&!lstrcmpW(buffer,L"Setup path"),"dialog Unicode text bridge");DestroyWindow(parent);
  check(GetModuleFileNameW(NULL,module,MAX_PATH)>0,"GetModuleFileNameW");lstrcpyW(command,L"\"");lstrcatW(command,module);lstrcatW(command,L"\" --child");ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);ZeroMemory(&pi,sizeof(pi));
  if(CreateProcessW(module,command,NULL,NULL,FALSE,0,NULL,dir,&si,&pi)){WaitForSingleObject(pi.hProcess,15000);GetExitCodeProcess(pi.hProcess,&exitCode);check(exitCode==37,"CreateProcessW child exit code and directory");CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}else check(FALSE,"CreateProcessW child");
  {WCHAR *message=NULL;n=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,ERROR_FILE_NOT_FOUND,0,(LPWSTR)&message,0,NULL);check(n>0&&message&&message[0],"allocated system error message");if(message)LocalFree(message);}
  if(GetVersion()&0x80000000UL){WCHAR bad[]={L'C',L':',L'\\',0x4e00,0};SetLastError(0);check(GetFileAttributesW(bad)==INVALID_FILE_ATTRIBUTES&&GetLastError()==ERROR_NO_UNICODE_TRANSLATION,"unrepresentable path rejected without replacement");}
  check(DeleteFileW(next)&&RemoveDirectoryW(dir),"fixture cleanup");check(failures==0,"ALL");CloseHandle(logFile);return failures?1:0;
}
