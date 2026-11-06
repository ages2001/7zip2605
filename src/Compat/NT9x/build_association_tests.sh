#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os -fno-builtin -fno-stack-protector -nostdlib \
 -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
 "$nt9x_source/Compat/NT9x/association_tests.c" \
 -lkernel32 -luser32 -ladvapi32 -lshell32 -o "$nt9x_build/ASSTEST.EXE"

for nt9x_runner in final_gui_runner gui_only_runner; do
  case "$nt9x_runner" in final_gui_runner) nt9x_output=FINAL23.EXE;; *) nt9x_output=GUIFINAL.EXE;; esac
  "$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os -fno-builtin -fno-stack-protector -nostdlib \
   -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
   "$nt9x_source/Compat/NT9x/$nt9x_runner.c" -lkernel32 -luser32 -o "$nt9x_build/$nt9x_output"
done
