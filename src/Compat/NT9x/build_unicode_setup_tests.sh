#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -mcrtdll=crtdll -Os -c "$nt9x_source/Compat/NT9x/setup_unicode_tests.c" -o "$nt9x_build/setup_unicode_tests.o"
printf '%s\n' "$nt9x_build/setup_unicode_tests.o" > "$nt9x_build/setup-unicode-tests.rsp"
NT9X_GUI_OUTPUT="$nt9x_build/SETUNIC.EXE" NT9X_GUI_OBJECTS="$nt9x_build/setup-unicode-tests.rsp" NT9X_GUI_MAP="$nt9x_build/setup-unicode-tests.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"
