/* Run the actual standalone setup, then open the OS File Manager.
   Disposable NT3/NT4 guests only. The test directory is C:\TEST26. */
#define entry outbound_suite_entry
#include "outbound_drop_tests.c"
#undef entry
void entry(void)
{
  WCHAR ini[MAX_PATH],value[2*MAX_PATH],expected[MAX_PATH];
  CHAR system[MAX_PATH],dll[MAX_PATH],directory[MAX_PATH];
  HANDLE process;DWORD code,version=GetVersion();HMODULE module;
  logFile=CreateFileA("C:\\WFSETUP.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
  if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
  require(!(version&0x80000000UL),"native NT File Manager guest");
  process=start("C:\\TEST26\\SETUP.EXE","/S",NULL);
  require(WaitForSingleObject(process,180000)==WAIT_OBJECT_0&&GetExitCodeProcess(process,&code)&&!code,"standalone installer completes successfully");
  CloseHandle(process);
  lstrcpyA(directory,(version&255)==3?"C:\\7-Zip":"C:\\Program Files\\7-Zip");
  wsprintfA(dll,"%s\\7zWF.dll",directory);
  GetWindowsDirectoryW(ini,MAX_PATH);lstrcatW(ini,L"\\WINFILE.INI");
  GetPrivateProfileStringW(L"AddOns",L"7-Zip",L"",value,2*MAX_PATH,ini);
  MultiByteToWideChar(CP_ACP,0,dll,-1,expected,MAX_PATH);
  require(!lstrcmpiW(value,expected),"setup registers its embedded WinFile add-on");
  module=LoadLibraryA(dll);require(module!=NULL,"embedded add-on loads on the target OS");
  require(GetProcAddress(module,"FMExtensionProc")&&GetProcAddress(module,"FMExtensionProcW"),"both File Manager extension entry points exist");
  FreeLibrary(module);
  require(SetCurrentDirectoryA("C:\\TEST26"),"select disposable drop destination");
  GetSystemDirectoryA(system,MAX_PATH);lstrcatA(system,"\\WINFILE.EXE");
  process=start(system,"",NULL);CloseHandle(process);
  record("PASS ALL");CloseHandle(logFile);ExitProcess(0);
}
