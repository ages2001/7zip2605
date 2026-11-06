/* Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_NT9X_SETUP_SHORTCUTS_H
#define Z7_NT9X_SETUP_SHORTCUTS_H
#include <windows.h>
/* Keep platform selection independent of shell availability: NT 3.x and
   Win32s use Program Manager; Windows 95 and NT 4+ use Explorer links. */
DWORD Z7SetupPlatform(DWORD version);
BOOL Z7SetupUsesProgman(DWORD platform, DWORD major);
DWORD Z7SetupDefaultPath(char *path, DWORD capacity);
DWORD Z7SetupCreateShortcuts(const char *executable, const char *workingDirectory);
DWORD Z7SetupRemoveShortcuts(const char *executable);
DWORD Z7SetupShortcutsW(const WCHAR *directory, BOOL remove);
DWORD Z7SetupWinFileW(const WCHAR *directory, BOOL remove);
#endif
