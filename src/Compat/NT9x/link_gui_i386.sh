#!/usr/bin/env bash
set -euo pipefail
# Link only against the locally rebuilt i386 runtime libraries.
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_gui_output=${NT9X_GUI_OUTPUT:-$nt9x_build/7zFM-i386.exe}
nt9x_gui_objects=${NT9X_GUI_OBJECTS:-$nt9x_build/gui-i386/objects.rsp}
nt9x_gui_resource=${NT9X_GUI_RESOURCE:-$nt9x_build/gui-i386/resource.o}
nt9x_gui_map=${NT9X_GUI_MAP:-$nt9x_build/gui-i386-link.map}
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-g++-win32" \
  ${NT9X_LINK_MODE:--mwindows} -march=i386 -mcrtdll=crtdll -nostdlib -static \
  -Wl,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  -Wl,-Map,"$nt9x_gui_map" -o "$nt9x_gui_output" \
  "${NT9X_CRT_OBJECT:-$nt9x_tools/i386-crt/lib32/crt2.o}" "$nt9x_tools/i386-libgcc/crtbegin.o" \
  @"$nt9x_gui_objects" ${NT9X_DEF_FILE:-} "$nt9x_gui_resource" "$nt9x_build/legacy_gui.o" "$nt9x_build/legacy_menu.o" "$nt9x_build/legacy_images.o" "$nt9x_build/legacy_icons.o" "$nt9x_build/legacy_toolbar.o" "$nt9x_build/legacy_progress.o" "$nt9x_build/legacy_updown.o" ${NT9X_PROCESS_OBJECT-"$nt9x_build/legacy_process.o"} "$nt9x_build/legacy_property.o" "$nt9x_build/legacy_listview.o" "$nt9x_build/legacy_mpr.o" "$nt9x_build/legacy_ole.o" "$nt9x_build/legacy_shell.o" "$nt9x_build/legacy_shell_folder.o" "$nt9x_build/legacy_shell_execute.o" "$nt9x_build/legacy_shell_ops.o" "$nt9x_build/legacy_shell_menu.o" "$nt9x_build/legacy_shell_browse.o" "$nt9x_build/legacy_controls.o" "$nt9x_build/legacy_help.o" \
  "$nt9x_build/legacy_win32_i386.o" "$nt9x_build/atomic_i386.o" \
  -Wl,--start-group \
  "$nt9x_tools/i386-libstdcxx/libsupc++/.libs/libsupc++.a" \
  "$nt9x_tools/i386-libstdcxx/src/c++11/.libs/libc++11convenience.a" \
  "$nt9x_tools/i386-libstdcxx/src/c++98/.libs/libc++98convenience.a" \
  -L"$nt9x_tools/i386-crt/lib32" -lmingw32 -lmoldname -lmingwex -lcrtdll \
  "$nt9x_tools/i386-libgcc/libgcc.a" \
  -lkernel32 -luser32 -ladvapi32 -lshell32 -luuid -lole32 -lcomctl32 -lcomdlg32 -lgdi32 -lmpr -lwinspool -Wl,--end-group \
  "$nt9x_tools/i386-libgcc/crtend.o"
