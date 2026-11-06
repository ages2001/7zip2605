#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_dir="$nt9x_build/plugin-fixture"
mkdir -p "$nt9x_dir"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-g++-win32" -march=i386 -mcrtdll=crtdll -Os -DZ7_LEGACY_WIN32 -c "$nt9x_source/Compat/NT9x/test_hasher.cpp" -o "$nt9x_dir/test_hasher.o"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -mcrtdll=crtdll -Os -c "$nt9x_source/Compat/NT9x/legacy_dll_entry.c" -o "$nt9x_dir/entry.o"
printf 'EXPORTS\nGetHashers\nGetModuleProp\n' > "$nt9x_dir/exports.def"
printf '%s\n' "$nt9x_dir/test_hasher.o" "$nt9x_dir/entry.o" > "$nt9x_dir/objects.rsp"
NT9X_LINK_MODE='-shared -Wl,-e,_LegacyDllEntry@12' NT9X_CRT_OBJECT="$nt9x_tools/i386-crt/lib32/dllcrt2.o" NT9X_DEF_FILE="$nt9x_dir/exports.def" NT9X_GUI_OUTPUT="$nt9x_dir/TESTHASH.DLL" NT9X_GUI_OBJECTS="$nt9x_dir/objects.rsp" NT9X_GUI_MAP="$nt9x_dir/link.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"
"$nt9x_tools/python/bin/python" "$nt9x_source/Compat/NT9x/prepare_legacy_dll.py" "$nt9x_dir/TESTHASH.DLL"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-strip" "$nt9x_dir/TESTHASH.DLL"
