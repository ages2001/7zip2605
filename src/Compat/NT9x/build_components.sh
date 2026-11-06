#!/usr/bin/env bash
# Auxiliary executables use the same i386 ABI and compatibility implementation.
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
python3 "$nt9x_source/Compat/NT9x/generate_components.py"
for nt9x_name in gui-command explorer sfx-win sfx-con; do
 nt9x_dir="$nt9x_build/$nt9x_name-i386"
 make -C "$nt9x_dir" -j"${NT9X_JOBS:-4}"
 case $nt9x_name in
  gui-command) nt9x_folder=CPP/7zip/UI/GUI; nt9x_output=7zG.exe;;
  explorer) nt9x_folder=CPP/7zip/UI/Explorer; nt9x_output=7-zip.dll;;
  sfx-win) nt9x_folder=CPP/7zip/Bundles/SFXWin; nt9x_output=7z.sfx;;
  sfx-con) nt9x_folder=CPP/7zip/Bundles/SFXCon; nt9x_output=7zCon.sfx;;
 esac
 (cd "$nt9x_source/$nt9x_folder"
 "$nt9x_tools/root/usr/bin/i686-w64-mingw32-windres" --preprocessor="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I "$nt9x_tools/include-aliases" resource.rc "$nt9x_dir/resource.o")
 nt9x_mode=-mwindows;nt9x_crt=crt2.o;nt9x_def=
 if [[ $nt9x_name == explorer ]]; then
  nt9x_mode='-shared -Wl,-e,_LegacyDllEntry@12';nt9x_crt=dllcrt2.o;nt9x_def="$nt9x_source/CPP/7zip/UI/Explorer/Explorer.def"
  "$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" -march=i386 -Os -c "$nt9x_source/Compat/NT9x/legacy_dll_entry.c" -o "$nt9x_dir/legacy_dll_entry.o"
  printf '%s\n' "$nt9x_dir/legacy_dll_entry.o" >> "$nt9x_dir/objects.rsp"
 fi
 if [[ $nt9x_name == sfx-con ]]; then nt9x_mode=-mconsole;fi
 NT9X_LINK_MODE="$nt9x_mode" NT9X_CRT_OBJECT="$nt9x_tools/i386-crt/lib32/$nt9x_crt" NT9X_DEF_FILE="$nt9x_def" NT9X_GUI_OUTPUT="$nt9x_build/$nt9x_output" NT9X_GUI_OBJECTS="$nt9x_dir/objects.rsp" NT9X_GUI_RESOURCE="$nt9x_dir/resource.o" NT9X_GUI_MAP="$nt9x_dir/link.map" bash "$nt9x_source/Compat/NT9x/link_gui_i386.sh"
 if [[ $nt9x_name == explorer ]]; then
  "$nt9x_tools/python/bin/python" "$nt9x_source/Compat/NT9x/prepare_legacy_dll.py" "$nt9x_build/$nt9x_output"
 fi
done
