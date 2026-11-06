#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_cc="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32"
nt9x_objects=()
for nt9x_unit in legacy_shell legacy_shell_folder legacy_shell_execute legacy_shell_ops legacy_shell_menu legacy_shell_browse legacy_images legacy_icons legacy_ole atomic_i386; do
 nt9x_objects+=("$nt9x_build/$nt9x_unit.o")
done
"$nt9x_cc" -march=i386 -Os -fno-builtin -fno-stack-protector -nostdlib \
 -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
 "$nt9x_source/Compat/NT9x/shell_tests.c" "${nt9x_objects[@]}" "$nt9x_tools/i386-libgcc/libgcc.a" \
 -lkernel32 -luser32 -lgdi32 -ladvapi32 -lshell32 -luuid -lcomctl32 -o "$nt9x_build/SHELLTST.EXE"
