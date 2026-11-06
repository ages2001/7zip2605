#!/usr/bin/env bash
# Build the short setup and live-network fixtures; these are not shipped in bin/.
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
mkdir -p "$nt9x_build"
for nt9x_pair in 'setup_smoke_runner SETSMOKE' 'nt351_smoke_runner QUICK' 'network_smoke_tests NETSMOKE' 'revision24_runner REV24' 'gui_about_tests ABOUT24' 'visual25_runner VISUAL25' 'column_snapshot_tests COLSAVE'; do
 read -r nt9x_unit nt9x_name <<< "$nt9x_pair"
 "$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os -fno-builtin -fno-stack-protector -nostdlib \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/$nt9x_unit.c" "$nt9x_tools/i386-libgcc/libgcc.a" -lkernel32 -luser32 -ladvapi32 -lmpr -lgdi32 -o "$nt9x_build/$nt9x_name.EXE"
done
