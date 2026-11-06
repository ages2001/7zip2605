#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os -Wall -Wextra -Wno-misleading-indentation \
 -DNT9X_MPR_TEST -fno-builtin -fno-stack-protector -nostdlib \
 -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
 "$nt9x_source/Compat/NT9x/mpr_tests.c" "$nt9x_source/Compat/NT9x/legacy_mpr.c" \
 -lkernel32 -o "$nt9x_build/MPRTEST.EXE"
