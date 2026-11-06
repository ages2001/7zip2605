#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_cc="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32"
"$nt9x_cc" -march=i386 -mcrtdll=crtdll -Os -c "$nt9x_source/Compat/NT9x/icon_tests.c" -o "$nt9x_build/icon_tests.o"
printf '101 ICON "%s"\n' "$nt9x_source/CPP/7zip/UI/FileManager/FM.ico" > "$nt9x_build/icon_tests.rc"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-windres" --preprocessor="$nt9x_cc" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED "$nt9x_build/icon_tests.rc" "$nt9x_build/icon_tests_res.o"
printf '%s\n' "$nt9x_build/icon_tests.o" > "$nt9x_build/icon-tests.rsp"
NT9X_GUI_OUTPUT="$nt9x_build/ICONTEST.EXE" NT9X_GUI_OBJECTS="$nt9x_build/icon-tests.rsp" NT9X_GUI_RESOURCE="$nt9x_build/icon_tests_res.o" NT9X_GUI_MAP="$nt9x_build/icon-tests.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"
