#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os \
  -fno-builtin -fno-stack-protector -nostdlib \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/guest_tests.c" -lkernel32 -o "$nt9x_build/GTEST.EXE"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-g++-win32" -march=i386 -mcrtdll=crtdll -O2 -c \
  "$nt9x_source/Compat/NT9x/runtime_tests.cpp" -o "$nt9x_build/runtime_tests.o"
printf '%s\n' "$nt9x_build/runtime_tests.o" > "$nt9x_build/runtime-objects.rsp"
NT9X_OBJECTS="$nt9x_build/runtime-objects.rsp" NT9X_OUTPUT="$nt9x_build/RTTEST.EXE" \
  bash "$nt9x_source/Compat/NT9x/link_i386.sh"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-windres" \
  --preprocessor="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" \
  --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED \
  "$nt9x_source/Compat/NT9x/menu_tests.rc" "$nt9x_build/menu_tests_res.o"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os \
  -fno-builtin -fno-stack-protector -nostdlib \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/menu_tests.c" "$nt9x_source/Compat/NT9x/legacy_menu.c" "$nt9x_build/menu_tests_res.o" \
  -lkernel32 -luser32 -lgdi32 -o "$nt9x_build/MNTEST.EXE"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os \
  -fno-builtin -fno-stack-protector -nostdlib \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/control_tests.c" \
  "$nt9x_source/Compat/NT9x/legacy_controls.c" "$nt9x_source/Compat/NT9x/legacy_images.c" \
  "$nt9x_source/Compat/NT9x/legacy_listview.c" "$nt9x_source/Compat/NT9x/legacy_toolbar.c" "$nt9x_source/Compat/NT9x/legacy_progress.c" "$nt9x_source/Compat/NT9x/legacy_updown.c" "$nt9x_source/Compat/NT9x/legacy_gui.c" \
  "$nt9x_tools/i386-libgcc/libgcc.a" -lkernel32 -luser32 -lgdi32 -lcomctl32 \
  -o "$nt9x_build/CTTEST.EXE"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os \
  -fno-builtin -fno-stack-protector -nostdlib \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/process_tests.c" "$nt9x_source/Compat/NT9x/legacy_process.c" \
  -lkernel32 -o "$nt9x_build/PSTEST.EXE"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-windres" \
  --preprocessor="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" \
  --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED \
  "$nt9x_source/Compat/NT9x/property_tests.rc" "$nt9x_build/property_tests_res.o"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os \
  -fno-builtin -fno-stack-protector -nostdlib \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/property_tests.c" "$nt9x_source/Compat/NT9x/legacy_property.c" \
  "$nt9x_build/property_tests_res.o" "$nt9x_tools/i386-libgcc/libgcc.a" \
  -lkernel32 -luser32 -lgdi32 -lcomctl32 -o "$nt9x_build/PHTEST.EXE"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os \
  -fno-builtin -fno-stack-protector -nostdlib \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/metadata_tests.c" -lkernel32 -o "$nt9x_build/METATEST.EXE"
