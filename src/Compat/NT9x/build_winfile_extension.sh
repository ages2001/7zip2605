#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
mkdir -p "$nt9x_build"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" \
 ${NT9X_WINFILE_FLAGS:-} -DZ7_SHELL_PATHS_ONLY -march=i386 -Os -fno-builtin -fno-stack-protector -ffunction-sections -fdata-sections -nostdlib -shared \
 -Wl,-e,_DllMain@12,--gc-sections,--exclude-all-symbols,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
 "$nt9x_source/Compat/NT9x/winfile_extension.c" \
 "$nt9x_source/Compat/NT9x/legacy_shell_ops.c" "$nt9x_source/Compat/NT9x/legacy_shell.c" \
 "$nt9x_source/Compat/NT9x/atomic_i386.c" "$nt9x_source/Compat/NT9x/winfile_extension.def" \
 "$nt9x_tools/i386-libgcc/libgcc.a" \
 -lkernel32 -luser32 -lgdi32 -ladvapi32 -lshell32 -luuid -o "$nt9x_build/7zWF.dll"
