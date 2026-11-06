#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os -Wall -Wextra \
  -fno-builtin -fno-stack-protector -nostdlib \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/shortcut_tests.c" "$nt9x_source/Compat/NT9x/setup_shortcuts.c" \
  "$nt9x_source/Compat/NT9x/setup_paths.c" \
  "$nt9x_tools/i386-libgcc/libgcc.a" -lkernel32 -luser32 -ladvapi32 -lcrtdll \
  -o "$nt9x_build/SCTEST.EXE"
