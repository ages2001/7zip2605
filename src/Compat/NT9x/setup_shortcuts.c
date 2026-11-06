/* Installer shortcut support for NT 3.x, Windows 95 and NT 4+.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <ddeml.h>
#include <shlobj.h>
#include <string.h>
#include "setup_shortcuts.h"

static const GUID z7ShellLink = {0x00021401,0,0, {0xc0,0,0,0,0,0,0,0x46}};
static const GUID z7IShellLinkA = {0x000214ee,0,0, {0xc0,0,0,0,0,0,0,0x46}};
static const GUID z7IShellLinkW = {0x000214f9,0,0, {0xc0,0,0,0,0,0,0,0x46}};
static const GUID z7IPersistFile = {0x0000010b,0,0, {0xc0,0,0,0,0,0,0,0x46}};

BOOL Z7SetupUsesProgman(DWORD platform, DWORD major)
{
  return (platform == VER_PLATFORM_WIN32_NT || platform == VER_PLATFORM_WIN32s) && major == 3;
}

DWORD Z7SetupPlatform(DWORD version)
{
  if (!(version & 0x80000000UL)) return VER_PLATFORM_WIN32_NT;
  return LOBYTE(LOWORD(version)) == 3 ? VER_PLATFORM_WIN32s : VER_PLATFORM_WIN32_WINDOWS;
}

static HDDEDATA CALLBACK ddeCallback(UINT type, UINT format, HCONV conversation,
    HSZ string1, HSZ string2, HDDEDATA data, ULONG_PTR value1, ULONG_PTR value2)
{
  (void)type; (void)format; (void)conversation; (void)string1;
  (void)string2; (void)data; (void)value1; (void)value2;
  return 0;
}

static BOOL ddeExecute(HCONV conversation, const char *command)
{
  DWORD result;
  return DdeClientTransaction((LPBYTE)command, lstrlenA(command) + 1,
      conversation, 0, 0, XTYP_EXECUTE, 10000, &result) != 0;
}

/* Query the actual group before changing a named item. A registry marker
   alone would not detect a shortcut edited by the user after installation. */
static DWORD progmanOwnership(DWORD instance,HCONV conversation,const char *executable,const char *title)
{
  HSZ item=DdeCreateStringHandleA(instance,"7-Zip 26.05",CP_WINANSI);
  HDDEDATA data;DWORD size;const char *text;DWORD result=ERROR_FILE_NOT_FOUND;
  if(!item)return ERROR_NOT_ENOUGH_MEMORY;
  data=DdeClientTransaction(NULL,0,conversation,item,CF_TEXT,XTYP_REQUEST,10000,NULL);
  DdeFreeStringHandle(instance,item);
  if(!data)return ERROR_FILE_NOT_FOUND;
  text=(const char*)DdeAccessData(data,&size);
  /* NT 3.51 can include allocation padding after the CF_TEXT terminator.
     Require a terminator inside the supplied buffer, not in its last byte. */
  if(text&&size&&memchr(text,0,size)){
    const char *line=text;
    while(*line){
      const char *end=strchr(line,'\n');
      const char *name=line, *nameEnd, *target, *targetEnd;
      if(!end)end=line+lstrlenA(line);
      /* The first two CSV fields are title and command. Never use the icon
         path as proof of ownership after a user edits a shortcut command. */
      if(name<end&&*name=='"'){name++;nameEnd=memchr(name,'"',end-name);}
      else nameEnd=memchr(name,',',end-name);
      if(nameEnd&&(unsigned)(nameEnd-name)==(unsigned)lstrlenA(title)&&
          !_strnicmp(name,title,nameEnd-name)){
        target=nameEnd;
        if(target<end&&*target=='"')target++;
        if(target<end&&*target==',')target++;else {result=ERROR_INVALID_DATA;break;}
        if(target<end&&*target=='"'){target++;targetEnd=memchr(target,'"',end-target);}
        else targetEnd=memchr(target,',',end-target);
        result=ERROR_ALREADY_EXISTS;
        if(targetEnd&&(unsigned)(targetEnd-target)==(unsigned)lstrlenA(executable)&&
            !_strnicmp(target,executable,targetEnd-target))result=ERROR_SUCCESS;
        break;
      }
      line=*end?end+1:end;
    }
  }else result=ERROR_INVALID_DATA;
  if(text)DdeUnaccessData(data);DdeFreeDataHandle(data);return result;
}
static BOOL progmanGroupEmpty(DWORD instance,HCONV conversation)
{
  HSZ item=DdeCreateStringHandleA(instance,"7-Zip 26.05",CP_WINANSI);
  HDDEDATA data;DWORD size;const char *text,*p;BOOL empty=FALSE;
  if(!item)return FALSE;
  data=DdeClientTransaction(NULL,0,conversation,item,CF_TEXT,XTYP_REQUEST,10000,NULL);
  DdeFreeStringHandle(instance,item);if(!data)return FALSE;
  text=(const char*)DdeAccessData(data,&size);
  if(text&&size&&memchr(text,0,size)){
    p=strchr(text,'\n');
    if(p){p++;while(*p=='\r'||*p=='\n')p++;empty=!*p;}
  }
  if(text)DdeUnaccessData(data);DdeFreeDataHandle(data);return empty;
}
static DWORD progmanShortcut(const char *executable, const char *directory, BOOL remove)
{
  static const char *titles[2] = {"7-Zip File Manager", "Uninstall 7-Zip"};
  DWORD instance = 0, error, ownership[2];
  HSZ service;
  HCONV conversation;
  char command[3 * MAX_PATH + 128], uninstaller[MAX_PATH];
  const char *targets[2], *p, *slash;
  unsigned i, prefix;
  /* DDE has no escaping convention for literal quotation marks. */
  for (p = executable; *p; ++p)
    if (*p == '"' || *p == '[' || *p == ']') return ERROR_INVALID_NAME;
  for (p = directory; *p; ++p)
    if (*p == '"' || *p == '[' || *p == ']') return ERROR_INVALID_NAME;
  slash = strrchr(executable, '\\');
  if (!slash) return ERROR_BAD_PATHNAME;
  prefix = (unsigned)(slash - executable + 1);
  if (prefix + sizeof("Uninst.exe") > sizeof(uninstaller)) return ERROR_FILENAME_EXCED_RANGE;
  CopyMemory(uninstaller, executable, prefix);
  lstrcpyA(uninstaller + prefix, "Uninst.exe");
  targets[0] = executable;
  targets[1] = uninstaller;
  error = DdeInitializeA(&instance, ddeCallback, APPCMD_CLIENTONLY, 0);
  if (error != DMLERR_NO_ERROR) return ERROR_NOT_READY;
  service = DdeCreateStringHandleA(instance, "PROGMAN", CP_WINANSI);
  conversation = service ? DdeConnect(instance, service, service, NULL) : NULL;
  if (!conversation) error = ERROR_NOT_READY;
  else {
    /* Check both names before updating either item. Preserve user-edited
       targets, including an unrelated item named Uninstall 7-Zip. */
    error = ERROR_SUCCESS;
    for (i = 0; i < 2; ++i) {
      ownership[i] = progmanOwnership(instance, conversation, targets[i], titles[i]);
      if (ownership[i] != ERROR_SUCCESS && ownership[i] != ERROR_FILE_NOT_FOUND &&
          !(remove && ownership[i] == ERROR_ALREADY_EXISTS)) error = ownership[i];
    }
    if (!error && remove) {
      for (i = 0; i < 2 && !error; ++i) {
        if (ownership[i] != ERROR_SUCCESS) continue;
        wsprintfA(command, "[DeleteItem(\"%s\")]", titles[i]);
        if (!ddeExecute(conversation, "[ShowGroup(\"7-Zip 26.05\",1)]") ||
            !ddeExecute(conversation, command)) error = ERROR_WRITE_FAULT;
      }
      if (!error && progmanGroupEmpty(instance, conversation) &&
          !ddeExecute(conversation, "[DeleteGroup(\"7-Zip 26.05\")]")) error = ERROR_WRITE_FAULT;
    } else if (!error) {
      if (!ddeExecute(conversation, "[CreateGroup(\"7-Zip 26.05\")]") ||
          !ddeExecute(conversation, "[ShowGroup(\"7-Zip 26.05\",1)]")) error = ERROR_WRITE_FAULT;
      for (i = 0; i < 2 && !error; ++i) {
        if (ownership[i] == ERROR_SUCCESS) {
          wsprintfA(command, "[DeleteItem(\"%s\")]", titles[i]);
          if (!ddeExecute(conversation, command)) error = ERROR_WRITE_FAULT;
        }
        if (!error) {
          /* No /S: launching the shortcut must show the uninstall dialog. */
          /* Program Manager makes an item's working directory current
             before starting it and keeps it current afterwards; DOS cannot
             remove a current directory. The uninstaller therefore starts in
             the root of the installation drive, not in the directory that it
             removes. */
          char root[4];
          const char *workDir = directory;
          if (i == 1 && directory[0] && directory[1] == ':') {
            root[0] = directory[0]; root[1] = ':'; root[2] = '\\'; root[3] = 0;
            workDir = root;
          }
          wsprintfA(command, "[AddItem(\"%s\",\"%s\",\"%s\",0,-1,-1,\"%s\")]",
              targets[i], titles[i], targets[i], workDir);
          if (!ddeExecute(conversation, command)) error = ERROR_WRITE_FAULT;
        }
      }
    }
    DdeDisconnect(conversation);
  }
  if (service) DdeFreeStringHandle(instance, service);
  DdeUninitialize(instance);
  return error;
}

static DWORD shellFolder(const char *name, char *path)
{
  HKEY key;
  DWORD size = MAX_PATH, type = 0;
  LONG error = RegOpenKeyExA(HKEY_CURRENT_USER,
      "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
      0, KEY_QUERY_VALUE, &key);
  if (error != ERROR_SUCCESS) return error;
  ZeroMemory(path, MAX_PATH);
  error = RegQueryValueExA(key, name, NULL, &type, (LPBYTE)path, &size);
  RegCloseKey(key);
  if (error != ERROR_SUCCESS) return error;
  if (type != REG_SZ || size == 0 || size > MAX_PATH || path[size-1] != 0)
    return ERROR_INVALID_DATA;
  if (!(GetFileAttributesA(path) & FILE_ATTRIBUTE_DIRECTORY) ||
      GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
    return ERROR_PATH_NOT_FOUND;
  return ERROR_SUCCESS;
}

typedef HRESULT (WINAPI *InitializeFn)(LPVOID);
typedef void (WINAPI *UninitializeFn)(void);
typedef HRESULT (WINAPI *CreateFn)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID *);

static DWORD writeLink(CreateFn create, const char *linkPath,
    const char *executable, const char *directory,BOOL remove)
{
  IShellLinkA *link = NULL;
  IPersistFile *persist = NULL;
  WCHAR widePath[MAX_PATH];
  char oldTarget[MAX_PATH];
  WIN32_FIND_DATAA data;
  HRESULT result;
  if(remove&&GetFileAttributesA(linkPath)==INVALID_FILE_ATTRIBUTES)return ERROR_SUCCESS;
  if (!MultiByteToWideChar(CP_ACP, 0, linkPath, -1, widePath, MAX_PATH))
    return GetLastError();
  result = create(&z7ShellLink, NULL, CLSCTX_INPROC_SERVER,
      &z7IShellLinkA, (void **)&link);
  if (FAILED(result)) return ERROR_NOT_SUPPORTED;
  result = IShellLinkA_QueryInterface(link, &z7IPersistFile, (void **)&persist);
  if (SUCCEEDED(result) && GetFileAttributesA(linkPath) != INVALID_FILE_ATTRIBUTES) {
    result = IPersistFile_Load(persist, widePath, STGM_READ);
    if (SUCCEEDED(result)) {
      oldTarget[0] = 0;
      result = IShellLinkA_GetPath(link, oldTarget, MAX_PATH, &data, SLGP_RAWPATH);
      if (SUCCEEDED(result) && lstrcmpiA(oldTarget, executable))
        result = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
    }
  }
  if(remove){
    if(result==HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))result=S_OK;
    else if(SUCCEEDED(result)&&!DeleteFileA(linkPath))result=HRESULT_FROM_WIN32(GetLastError());
    if(persist)IPersistFile_Release(persist);IShellLinkA_Release(link);
    return SUCCEEDED(result)?ERROR_SUCCESS:ERROR_WRITE_FAULT;
  }
  if (SUCCEEDED(result)) result = IShellLinkA_SetPath(link, executable);
  if (SUCCEEDED(result)) result = IShellLinkA_SetArguments(link, "");
  if (SUCCEEDED(result)) result = IShellLinkA_SetWorkingDirectory(link, directory);
  if (SUCCEEDED(result)) result = IShellLinkA_SetDescription(link, "7-Zip File Manager");
  if (SUCCEEDED(result)) result = IShellLinkA_SetIconLocation(link, executable, 0);
  if (SUCCEEDED(result)) result = IShellLinkA_SetShowCmd(link, SW_SHOWNORMAL);
  if (SUCCEEDED(result)) result = IPersistFile_Save(persist, widePath, TRUE);
  if (persist) IPersistFile_Release(persist);
  IShellLinkA_Release(link);
  return SUCCEEDED(result) ? ERROR_SUCCESS :
      (HRESULT_FACILITY(result) == FACILITY_WIN32 ? HRESULT_CODE(result) : ERROR_WRITE_FAULT);
}

static DWORD explorerShortcuts(const char *executable, const char *directory,BOOL remove)
{
  char desktop[MAX_PATH], programs[MAX_PATH];
  const char *suffix = "\\7-Zip File Manager.lnk";
  HMODULE ole;
  InitializeFn initialize;
  UninitializeFn uninitialize;
  CreateFn create;
  HRESULT result;
  DWORD error = shellFolder("Desktop", desktop);
  if (!error) error = shellFolder("Programs", programs);
  if (error) return error;
  if (lstrlenA(desktop) + lstrlenA(suffix) >= MAX_PATH ||
      lstrlenA(programs) + lstrlenA(suffix) >= MAX_PATH)
    return ERROR_FILENAME_EXCED_RANGE;
  lstrcatA(desktop, suffix);
  lstrcatA(programs, suffix);
  /* NT 3.x never loads OLE32, which is absent on NT 3.1. */
  ole = LoadLibraryA("OLE32.DLL");
  if (!ole) return GetLastError();
  initialize = (InitializeFn)GetProcAddress(ole, "CoInitialize");
  uninitialize = (UninitializeFn)GetProcAddress(ole, "CoUninitialize");
  create = (CreateFn)GetProcAddress(ole, "CoCreateInstance");
  error = ERROR_PROC_NOT_FOUND;
  if (initialize && uninitialize && create) {
    result = initialize(NULL);
    if (SUCCEEDED(result)) {
      error = writeLink(create, desktop, executable, directory,remove);
      if (!error) error = writeLink(create, programs, executable, directory,remove);
      uninitialize();
    } else error = ERROR_NOT_READY;
  }
  FreeLibrary(ole);
  return error;
}

/* NT4 shell links retain UTF-16 paths, including names outside the ACP. */
static DWORD shellFolderW(const WCHAR *name, WCHAR *path)
{
  HKEY key;
  DWORD size = MAX_PATH * sizeof(WCHAR), type = 0;
  LONG error = RegOpenKeyExW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
      0, KEY_QUERY_VALUE, &key);
  if (error != ERROR_SUCCESS) return error;
  ZeroMemory(path, MAX_PATH * sizeof(WCHAR));
  error = RegQueryValueExW(key, name, NULL, &type, (LPBYTE)path, &size);
  RegCloseKey(key);
  if (error != ERROR_SUCCESS) return error;
  if (type != REG_SZ || size == 0 || size > MAX_PATH * sizeof(WCHAR) || (size & 1) || path[size/2-1] != 0)
    return ERROR_INVALID_DATA;
  if (!(GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY) ||
      GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
    return ERROR_PATH_NOT_FOUND;
  return ERROR_SUCCESS;
}

static DWORD writeLinkW(CreateFn create, const WCHAR *linkPath,
    const WCHAR *executable, const WCHAR *directory,BOOL remove)
{
  IShellLinkW *link = NULL;
  IPersistFile *persist = NULL;
  WCHAR oldTarget[MAX_PATH];
  WIN32_FIND_DATAW data;
  HRESULT result;
  if(remove&&GetFileAttributesW(linkPath)==INVALID_FILE_ATTRIBUTES)return ERROR_SUCCESS;
  result = create(&z7ShellLink, NULL, CLSCTX_INPROC_SERVER,
      &z7IShellLinkW, (void **)&link);
  if (FAILED(result)) return ERROR_NOT_SUPPORTED;
  result = IShellLinkW_QueryInterface(link, &z7IPersistFile, (void **)&persist);
  if (SUCCEEDED(result) && GetFileAttributesW(linkPath) != INVALID_FILE_ATTRIBUTES) {
    result = IPersistFile_Load(persist, linkPath, STGM_READ);
    if (SUCCEEDED(result)) {
      oldTarget[0] = 0;
      result = IShellLinkW_GetPath(link, oldTarget, MAX_PATH, &data, SLGP_RAWPATH);
      if (SUCCEEDED(result) && lstrcmpiW(oldTarget, executable))
        result = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
    }
  }
  if(remove){
    if(result==HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))result=S_OK;
    else if(SUCCEEDED(result)&&!DeleteFileW(linkPath))result=HRESULT_FROM_WIN32(GetLastError());
    if(persist)IPersistFile_Release(persist);IShellLinkW_Release(link);
    return SUCCEEDED(result)?ERROR_SUCCESS:ERROR_WRITE_FAULT;
  }
  if (SUCCEEDED(result)) result = IShellLinkW_SetPath(link, executable);
  if (SUCCEEDED(result)) result = IShellLinkW_SetArguments(link, L"");
  if (SUCCEEDED(result)) result = IShellLinkW_SetWorkingDirectory(link, directory);
  if (SUCCEEDED(result)) result = IShellLinkW_SetDescription(link, L"7-Zip File Manager");
  if (SUCCEEDED(result)) result = IShellLinkW_SetIconLocation(link, executable, 0);
  if (SUCCEEDED(result)) result = IShellLinkW_SetShowCmd(link, SW_SHOWNORMAL);
  if (SUCCEEDED(result)) result = IPersistFile_Save(persist, linkPath, TRUE);
  if (persist) IPersistFile_Release(persist);
  IShellLinkW_Release(link);
  return SUCCEEDED(result) ? ERROR_SUCCESS :
      (HRESULT_FACILITY(result) == FACILITY_WIN32 ? HRESULT_CODE(result) : ERROR_WRITE_FAULT);
}

static DWORD explorerShortcutsW(const WCHAR *executable, const WCHAR *directory,BOOL remove)
{
  WCHAR desktop[MAX_PATH], programs[MAX_PATH];
  const WCHAR *suffix = L"\\7-Zip File Manager.lnk";
  HMODULE ole;
  InitializeFn initialize;
  UninitializeFn uninitialize;
  CreateFn create;
  HRESULT result;
  DWORD error = shellFolderW(L"Desktop", desktop);
  if (!error) error = shellFolderW(L"Programs", programs);
  if (error) return error;
  if (lstrlenW(desktop) + lstrlenW(suffix) >= MAX_PATH ||
      lstrlenW(programs) + lstrlenW(suffix) >= MAX_PATH)
    return ERROR_FILENAME_EXCED_RANGE;
  lstrcatW(desktop, suffix);
  lstrcatW(programs, suffix);
  /* NT 3.x never loads OLE32, which is absent on NT 3.1. */
  ole = LoadLibraryA("OLE32.DLL");
  if (!ole) return GetLastError();
  initialize = (InitializeFn)GetProcAddress(ole, "CoInitialize");
  uninitialize = (UninitializeFn)GetProcAddress(ole, "CoUninitialize");
  create = (CreateFn)GetProcAddress(ole, "CoCreateInstance");
  error = ERROR_PROC_NOT_FOUND;
  if (initialize && uninitialize && create) {
    result = initialize(NULL);
    if (SUCCEEDED(result)) {
      error = writeLinkW(create, desktop, executable, directory,remove);
      if (!error) error = writeLinkW(create, programs, executable, directory,remove);
      uninitialize();
    } else error = ERROR_NOT_READY;
  }
  FreeLibrary(ole);
  return error;
}


DWORD Z7SetupCreateShortcuts(const char *executable, const char *workingDirectory)
{
  DWORD version, platform;
  DWORD attributes;
  if (!executable || !workingDirectory) return ERROR_INVALID_PARAMETER;
  if (lstrlenA(executable) >= MAX_PATH || lstrlenA(workingDirectory) >= MAX_PATH)
    return ERROR_FILENAME_EXCED_RANGE;
  /* Require a rooted drive or UNC path, never an installer-relative target. */
  if (!((executable[0] && executable[1] == ':' && executable[2] == '\\') ||
      (executable[0] == '\\' && executable[1] == '\\')))
    return ERROR_BAD_PATHNAME;
  attributes = GetFileAttributesA(executable);
  if (attributes == INVALID_FILE_ATTRIBUTES) return GetLastError();
  if (attributes & FILE_ATTRIBUTE_DIRECTORY) return ERROR_DIRECTORY;
  attributes = GetFileAttributesA(workingDirectory);
  if (attributes == INVALID_FILE_ATTRIBUTES) return GetLastError();
  if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
  version = GetVersion();
  platform = Z7SetupPlatform(version);
  return Z7SetupUsesProgman(platform, LOBYTE(LOWORD(version)))
      ? progmanShortcut(executable, workingDirectory,FALSE)
      : explorerShortcuts(executable, workingDirectory,FALSE);
}

DWORD Z7SetupRemoveShortcuts(const char *executable)
{
  DWORD version=GetVersion();
  if(!executable||lstrlenA(executable)>=MAX_PATH)return ERROR_INVALID_PARAMETER;
  return Z7SetupUsesProgman(Z7SetupPlatform(version),LOBYTE(LOWORD(version)))
      ?progmanShortcut(executable,"",TRUE):explorerShortcuts(executable,"",TRUE);
}
DWORD Z7SetupShortcutsW(const WCHAR *directory,BOOL remove)
{
  char dir[MAX_PATH],exe[MAX_PATH];BOOL used=FALSE;unsigned length;
  DWORD version=GetVersion();
  if(!(version&0x80000000UL)&&(version&255)>=4){
    WCHAR dirW[MAX_PATH],exeW[MAX_PATH];DWORD attrs;
    if(!directory)return ERROR_INVALID_PARAMETER;
    length=lstrlenW(directory);if(length>=MAX_PATH)return ERROR_FILENAME_EXCED_RANGE;
    lstrcpyW(dirW,directory);while(length&&dirW[length-1]=='\\')dirW[--length]=0;
    if(length+10>=MAX_PATH)return ERROR_FILENAME_EXCED_RANGE;
    lstrcpyW(exeW,dirW);lstrcatW(exeW,L"\\7zFM.exe");
    if(!remove){attrs=GetFileAttributesW(exeW);if(attrs==INVALID_FILE_ATTRIBUTES)return GetLastError();if(attrs&FILE_ATTRIBUTE_DIRECTORY)return ERROR_DIRECTORY;}
    return explorerShortcutsW(exeW,dirW,remove);
  }
  if(!WideCharToMultiByte(CP_ACP,0,directory,-1,dir,MAX_PATH,NULL,&used)||used)return ERROR_NO_UNICODE_TRANSLATION;
  length=lstrlenA(dir);while(length&&dir[length-1]=='\\')dir[--length]=0;
  if(length+10>=MAX_PATH)return ERROR_FILENAME_EXCED_RANGE;
  lstrcpyA(exe,dir);lstrcatA(exe,"\\7zFM.exe");
  return remove?Z7SetupRemoveShortcuts(exe):Z7SetupCreateShortcuts(exe,dir);
}
