#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_cc="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32"
mkdir -p "$nt9x_build"
"$nt9x_cc" -march=i386 -Os -fno-builtin -fno-stack-protector -nostdlib -shared \
 -Wl,-e,_DllMain@12,--kill-at,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
 "$nt9x_source/Compat/NT9x/ole_test_class.c" "$nt9x_source/Compat/NT9x/atomic_i386.c" -lkernel32 -luuid -o "$nt9x_build/CLASS.DLL"
"$nt9x_cc" -march=i386 -Os -Wall -Wextra -Wno-misleading-indentation -Wno-cast-function-type \
 -fno-builtin -fno-stack-protector -nostdlib \
 -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
 "$nt9x_source/Compat/NT9x/ole_tests.c" "$nt9x_source/Compat/NT9x/atomic_i386.c" "$nt9x_tools/i386-libgcc/libgcc.a" \
 -lkernel32 -luser32 -lgdi32 -ladvapi32 -lshell32 -luuid -o "$nt9x_build/OLETEST.EXE"
for nt9x_pair in 'outbound_drop_tests OUTDROP' 'file_drop_protocol_tests DROP57' 'winfile_drop_tests WFDROP' 'winfile_drag_suite WFD57' 'winfile_targets_tests WFT58' 'winfile_setup_tests WFSETUP' 'winfile_remove_tests WFREMOVE' 'winfile_reload_tests WFRELOAD'; do
 read -r nt9x_unit nt9x_name <<< "$nt9x_pair"
 nt9x_extra=()
 if [[ $nt9x_unit == winfile_remove_tests ]]; then nt9x_extra+=("$nt9x_source/Compat/NT9x/setup_winfile.c"); fi
 "$nt9x_cc" -march=i386 -Os -fno-builtin -fno-stack-protector -nostdlib -mwindows \
  -Wl,-e,_entry,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  "$nt9x_source/Compat/NT9x/$nt9x_unit.c" "${nt9x_extra[@]}" "$nt9x_source/Compat/NT9x/atomic_i386.c" "$nt9x_tools/i386-libgcc/libgcc.a" \
  -lkernel32 -luser32 -lgdi32 -ladvapi32 -lshell32 -luuid -o "$nt9x_build/$nt9x_name.EXE"
done
