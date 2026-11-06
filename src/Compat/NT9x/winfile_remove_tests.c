/* Native registration ownership and actual uninstall/reinstall regression. */
#define entry outbound_suite_entry
#include "outbound_drop_tests.c"
#undef entry
#include "setup_shortcuts.h"
void entry(void)
{
  WCHAR ini[MAX_PATH],directory[MAX_PATH],owned[MAX_PATH],value[2*MAX_PATH];
  CHAR path[MAX_PATH],dll[MAX_PATH];HANDLE process;DWORD code,begin;
  HWND winfile=FindWindowA(NULL,"File Manager");
  logFile=CreateFileA("C:\\WFREMOVE.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
  if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
  if(winfile)SendMessageA(winfile,WM_CLOSE,0,0);
  process=start("C:\\TEST26\\SETUP.EXE","/S",NULL);
  require(WaitForSingleObject(process,180000)==WAIT_OBJECT_0&&GetExitCodeProcess(process,&code)&&!code,"prepare installed package for ownership regression");CloseHandle(process);
  lstrcpyW(directory,(GetVersion()&255)==3?L"C:\\7-Zip":L"C:\\Program Files\\7-Zip");
  GetWindowsDirectoryW(ini,MAX_PATH);lstrcatW(ini,L"\\WINFILE.INI");
  GetPrivateProfileStringW(L"AddOns",L"7-Zip",L"",owned,MAX_PATH,ini);
  require(*owned!=0,"owned add-on registration exists after actual setup");
  require(WritePrivateProfileStringW(L"AddOns",L"7-Zip",L"C:\\Other\\User.dll",ini),"simulate user-edited add-on registration");
  require(Z7SetupWinFileW(directory,TRUE)==ERROR_SUCCESS,"unregister preserves a different owner");
  GetPrivateProfileStringW(L"AddOns",L"7-Zip",L"",value,2*MAX_PATH,ini);
  require(!lstrcmpW(value,L"C:\\Other\\User.dll"),"user-edited registration remains unchanged");
  require(Z7SetupWinFileW(directory,FALSE)==ERROR_ALREADY_EXISTS,"installation reports an add-on name collision");
  require(WritePrivateProfileStringW(L"AddOns",L"7-Zip",owned,ini),"restore owned test registration");
  WideCharToMultiByte(CP_ACP,0,owned,-1,dll,MAX_PATH,0,0);
  WideCharToMultiByte(CP_ACP,0,directory,-1,path,MAX_PATH,0,0);lstrcatA(path,"\\Uninst.exe");
  process=start(path,"/S",NULL);
  require(WaitForSingleObject(process,180000)==WAIT_OBJECT_0&&GetExitCodeProcess(process,&code)&&!code,"actual uninstaller completes successfully");CloseHandle(process);
  /* The uninstaller relaunches a temporary child. Parent exit and DLL
     deletion can precede profile cleanup, especially on NT 3.1. */
  begin=GetTickCount();do {
    GetPrivateProfileStringW(L"AddOns",L"7-Zip",L"",value,2*MAX_PATH,ini);
    if(GetFileAttributesA(dll)==INVALID_FILE_ATTRIBUTES&&!*value)break;
    Sleep(100);
  }while(GetTickCount()-begin<60000);
  require(GetFileAttributesA(dll)==INVALID_FILE_ATTRIBUTES,"actual uninstall removes the add-on DLL");
  GetPrivateProfileStringW(L"AddOns",L"7-Zip",L"",value,2*MAX_PATH,ini);
  require(!*value,"actual uninstall removes its owned WinFile registration");
  process=start("C:\\TEST26\\SETUP.EXE","/S",NULL);
  require(WaitForSingleObject(process,180000)==WAIT_OBJECT_0&&GetExitCodeProcess(process,&code)&&!code,"standalone reinstall succeeds after removal");CloseHandle(process);
  GetPrivateProfileStringW(L"AddOns",L"7-Zip",L"",value,2*MAX_PATH,ini);
  require(!lstrcmpiW(value,owned)&&GetFileAttributesA(dll)!=INVALID_FILE_ATTRIBUTES,"reinstall restores DLL and registration");
  record("PASS ALL");CloseHandle(logFile);ExitProcess(0);
}
