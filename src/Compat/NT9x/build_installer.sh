#!/usr/bin/env bash
# Build the upstream installer/uninstaller with the same i386 compatibility ABI.
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_cc="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32"
nt9x_flags=(-march=i386 -mcrtdll=crtdll -Os -DZ7_LEGACY_WIN32 -DZ7_NO_UNICODE -DZ7_WIN32_WINNT_MIN=0x030A -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -DZ7_LZMA_SIZE_OPT -DZ7_NO_METHOD_LZMA2 -DZ7_NO_METHODS_FILTERS -DZ7_USE_NATIVE_BRANCH_FILTER -DZ7_EXTRACT_ONLY)
if [[ ${NT9X_SETUP_DIAGNOSTICS:-0} == 1 ]]; then nt9x_flags+=(-DNT9X_SETUP_DIAGNOSTICS); fi
mkdir -p "$nt9x_build/setup-i386"
for nt9x_name in 7zipInstall 7zipUninstall; do
 nt9x_dir="$nt9x_build/setup-i386/$nt9x_name"
 mkdir -p "$nt9x_dir"
 "$nt9x_cc" "${nt9x_flags[@]}" -c "$nt9x_source/C/Util/$nt9x_name/$nt9x_name.c" -o "$nt9x_dir/main.o"
 printf '%s\n' "$nt9x_dir/main.o" > "$nt9x_dir/objects.rsp"
 for nt9x_unit in setup_win95 setup_shortcuts setup_paths setup_winfile; do
  "$nt9x_cc" "${nt9x_flags[@]}" -c "$nt9x_source/Compat/NT9x/$nt9x_unit.c" -o "$nt9x_dir/$nt9x_unit.o"
  printf '%s\n' "$nt9x_dir/$nt9x_unit.o" >> "$nt9x_dir/objects.rsp"
 done
 if [[ $nt9x_name == 7zipInstall ]]; then
  for nt9x_unit in 7zAlloc 7zArcIn 7zBuf 7zFile 7zDec 7zStream Bcj2 Bra CpuArch DllSecur LzmaDec 7zCrc 7zCrcOpt; do
   "$nt9x_cc" "${nt9x_flags[@]}" -c "$nt9x_source/C/$nt9x_unit.c" -o "$nt9x_dir/$nt9x_unit.o"
   printf '%s\n' "$nt9x_dir/$nt9x_unit.o" >> "$nt9x_dir/objects.rsp"
  done
 fi
 (cd "$nt9x_source/C/Util/$nt9x_name"
 "$nt9x_tools/root/usr/bin/i686-w64-mingw32-windres" --preprocessor="$nt9x_cc" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I "$nt9x_tools/include-aliases" resource.rc "$nt9x_dir/resource.o")
 NT9X_PROCESS_OBJECT= NT9X_GUI_OUTPUT="$nt9x_build/$nt9x_name-i386.exe" NT9X_GUI_OBJECTS="$nt9x_dir/objects.rsp" NT9X_GUI_RESOURCE="$nt9x_dir/resource.o" NT9X_GUI_MAP="$nt9x_dir/link.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"
done

"$nt9x_cc" "${nt9x_flags[@]}" -c "$nt9x_source/Compat/NT9x/setup_api_tests.c" -o "$nt9x_build/setup-i386/setup_api_tests.o"
printf '%s\n' "$nt9x_build/setup-i386/setup_api_tests.o" "$nt9x_build/setup-i386/7zipInstall/setup_win95.o" > "$nt9x_build/setup-i386/tests.rsp"
NT9X_PROCESS_OBJECT= NT9X_GUI_OUTPUT="$nt9x_build/SETAPI.EXE" NT9X_GUI_OBJECTS="$nt9x_build/setup-i386/tests.rsp" NT9X_GUI_RESOURCE="$nt9x_build/setup-i386/7zipInstall/resource.o" NT9X_GUI_MAP="$nt9x_build/setup-i386/tests.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"

"$nt9x_cc" "${nt9x_flags[@]}" -c "$nt9x_source/Compat/NT9x/setup_integration_tests.c" -o "$nt9x_build/setup-i386/setup_integration_tests.o"
printf '%s\n' "$nt9x_build/setup-i386/setup_integration_tests.o" "$nt9x_build/setup-i386/7zipInstall/setup_win95.o" "$nt9x_build/setup-i386/7zipInstall/setup_shortcuts.o" "$nt9x_build/setup-i386/7zipInstall/setup_paths.o" > "$nt9x_build/setup-i386/integration.rsp"
NT9X_PROCESS_OBJECT= NT9X_GUI_OUTPUT="$nt9x_build/SETTEST.EXE" NT9X_GUI_OBJECTS="$nt9x_build/setup-i386/integration.rsp" NT9X_GUI_RESOURCE="$nt9x_build/setup-i386/7zipInstall/resource.o" NT9X_GUI_MAP="$nt9x_build/setup-i386/integration.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"

"$nt9x_cc" "${nt9x_flags[@]}" -c "$nt9x_source/Compat/NT9x/progman_ownership_tests.c" -o "$nt9x_build/setup-i386/progman_ownership_tests.o"
sed 's@setup_integration_tests.o@progman_ownership_tests.o@' "$nt9x_build/setup-i386/integration.rsp" > "$nt9x_build/setup-i386/ownership.rsp"
NT9X_PROCESS_OBJECT= NT9X_GUI_OUTPUT="$nt9x_build/OWN24.EXE" NT9X_GUI_OBJECTS="$nt9x_build/setup-i386/ownership.rsp" NT9X_GUI_RESOURCE="$nt9x_build/setup-i386/7zipInstall/resource.o" NT9X_GUI_MAP="$nt9x_build/setup-i386/ownership.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"
