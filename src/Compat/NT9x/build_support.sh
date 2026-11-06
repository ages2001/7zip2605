#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_manual=${NT9X_MANUAL:-$nt9x_tools/manual/html}
nt9x_cc="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32"
mkdir -p "$nt9x_build/gui-i386"
for nt9x_unit in legacy_win32 atomic_i386 legacy_gui legacy_controls legacy_menu legacy_images legacy_icons legacy_toolbar legacy_progress legacy_updown legacy_process legacy_property legacy_listview legacy_mpr legacy_ole legacy_shell legacy_shell_folder legacy_shell_execute legacy_shell_ops legacy_shell_menu legacy_shell_browse; do
  nt9x_output=$nt9x_unit
  if [[ $nt9x_unit == legacy_win32 ]]; then nt9x_output=legacy_win32_i386; fi
  "$nt9x_cc" -march=i386 -mcrtdll=crtdll -O2 -c \
    "$nt9x_source/Compat/NT9x/$nt9x_unit.c" -o "$nt9x_build/$nt9x_output.o"
done
python3 "$nt9x_source/Compat/NT9x/make_help.py" "$nt9x_manual" "$nt9x_build/help"
"$nt9x_cc" -march=i386 -mcrtdll=crtdll -O2 -I"$nt9x_build/help" -c \
  "$nt9x_source/Compat/NT9x/legacy_help.c" -o "$nt9x_build/legacy_help.o"
printf '#include "%s"\n#include "%s"\n' \
  "$nt9x_source/CPP/7zip/Bundles/Fm/resource.rc" "$nt9x_build/help/help.rc" \
  > "$nt9x_build/gui-i386/combined.rc"
cd "$nt9x_source/CPP/7zip/Bundles/Fm"
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-windres" \
  --preprocessor="$nt9x_cc" --preprocessor-arg=-E --preprocessor-arg=-xc \
  --preprocessor-arg=-DRC_INVOKED -I "$nt9x_tools/include-aliases" \
  "$nt9x_build/gui-i386/combined.rc" "$nt9x_build/gui-i386/resource.o"
