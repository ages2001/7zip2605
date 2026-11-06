/* Default installation paths for the legacy installer.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "setup_shortcuts.h"

DWORD Z7SetupDefaultPath(char *path, DWORD capacity)
{
  DWORD version = GetVersion();
  DWORD platform = Z7SetupPlatform(version);
  DWORD type = 0, size = MAX_PATH;
  char root[MAX_PATH], expanded[MAX_PATH];
  HKEY key;
  LONG error;
  unsigned length;
  if (!path || !capacity) return ERROR_INVALID_PARAMETER;
  path[0] = 0;
  if (Z7SetupUsesProgman(platform, LOBYTE(LOWORD(version)))) {
    if (capacity < sizeof("C:\\7-Zip")) return ERROR_INSUFFICIENT_BUFFER;
    lstrcpyA(path, "C:\\7-Zip");
    return ERROR_SUCCESS;
  }
  error = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
      "Software\\Microsoft\\Windows\\CurrentVersion", 0, KEY_QUERY_VALUE, &key);
  if (error == ERROR_SUCCESS) {
    error = RegQueryValueExA(key, "ProgramFilesDir", NULL, &type, (LPBYTE)root, &size);
    RegCloseKey(key);
  }
  if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
    /* Early Windows 95 installations may omit ProgramFilesDir. Their
       conventional location is on the Windows drive, not necessarily C:. */
    UINT count = GetWindowsDirectoryA(root, MAX_PATH);
    if (!count || count >= MAX_PATH || root[1] != ':') return ERROR_BAD_PATHNAME;
    lstrcpyA(root + 2, "\\Program Files");
  } else {
    if (error != ERROR_SUCCESS) return error;
    if ((type != REG_SZ && type != REG_EXPAND_SZ) || size == 0 ||
        size > MAX_PATH || root[size - 1] != 0 || !root[0]) return ERROR_INVALID_DATA;
    if (type == REG_EXPAND_SZ) {
      DWORD count = ExpandEnvironmentStringsA(root, expanded, MAX_PATH);
      if (!count) return GetLastError();
      if (count > MAX_PATH) return ERROR_FILENAME_EXCED_RANGE;
      lstrcpyA(root, expanded);
    }
  }
  length = lstrlenA(root);
  while (length && (root[length - 1] == '\\' || root[length - 1] == '/'))
    root[--length] = 0;
  if (!((length >= 2 && root[1] == ':' &&
         (length == 2 || root[2] == '\\' || root[2] == '/')) ||
        (length >= 3 && root[0] == '\\' && root[1] == '\\')))
    return ERROR_BAD_PATHNAME;
  if (length + sizeof("\\7-Zip") > MAX_PATH ||
      length + sizeof("\\7-Zip") > capacity) return ERROR_INSUFFICIENT_BUFFER;
  lstrcpyA(path, root);
  lstrcatA(path, "\\7-Zip");
  return ERROR_SUCCESS;
}
