#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_cc="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32"
"$nt9x_cc" -march=i386 -mcrtdll=crtdll -Os -c "$nt9x_source/Compat/NT9x/component_tests.c" -o "$nt9x_build/component_tests.o"
printf '%s\n' "$nt9x_build/component_tests.o" "$nt9x_build/setup-i386/7zipInstall/setup_win95.o" "$nt9x_build/setup-i386/7zipInstall/setup_shortcuts.o" "$nt9x_build/setup-i386/7zipInstall/setup_paths.o" > "$nt9x_build/component-tests.rsp"
NT9X_PROCESS_OBJECT= NT9X_GUI_OUTPUT="$nt9x_build/COMPTEST.EXE" NT9X_GUI_OBJECTS="$nt9x_build/component-tests.rsp" NT9X_GUI_RESOURCE="$nt9x_build/setup-i386/7zipInstall/resource.o" NT9X_GUI_MAP="$nt9x_build/component-tests.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"
