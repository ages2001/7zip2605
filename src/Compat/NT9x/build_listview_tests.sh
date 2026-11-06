#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os -Wall -Wextra -Wno-misleading-indentation \
 -fno-builtin -fno-stack-protector -nostdlib \
 -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
 "$nt9x_source/Compat/NT9x/listview_tests.c" "$nt9x_source/Compat/NT9x/legacy_listview.c" \
 "$nt9x_source/Compat/NT9x/legacy_controls.c" "$nt9x_source/Compat/NT9x/legacy_images.c" \
 "$nt9x_source/Compat/NT9x/legacy_toolbar.c" "$nt9x_source/Compat/NT9x/legacy_progress.c" "$nt9x_source/Compat/NT9x/legacy_updown.c" "$nt9x_source/Compat/NT9x/legacy_gui.c" \
 "$nt9x_tools/i386-libgcc/libgcc.a" -lkernel32 -luser32 -lgdi32 -lcomctl32 \
 -o "$nt9x_build/LVTEST.EXE"
