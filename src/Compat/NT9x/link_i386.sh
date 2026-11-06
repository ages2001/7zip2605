#!/usr/bin/env bash
set -euo pipefail
# Link only against the locally rebuilt i386 runtime libraries.
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_objects=${NT9X_OBJECTS:-$nt9x_build/i386-objects.rsp}
nt9x_output=${NT9X_OUTPUT:-$nt9x_build/7zz-i386.exe}
"$nt9x_tools/root/usr/bin/i686-w64-mingw32-g++-win32" \
  -march=i386 -mcrtdll=crtdll -nostdlib -static \
  -Wl,--major-os-version,3,--minor-os-version,10,--major-subsystem-version,3,--minor-subsystem-version,10 \
  -Wl,-Map,"$nt9x_output.map" -o "$nt9x_output" \
  "$nt9x_tools/i386-crt/lib32/crt2.o" "$nt9x_tools/i386-libgcc/crtbegin.o" \
  @"$nt9x_objects" \
  "$nt9x_build/legacy_win32_i386.o" "$nt9x_build/atomic_i386.o" \
  -Wl,--start-group \
  "$nt9x_tools/i386-libstdcxx/libsupc++/.libs/libsupc++.a" \
  "$nt9x_tools/i386-libstdcxx/src/c++11/.libs/libc++11convenience.a" \
  "$nt9x_tools/i386-libstdcxx/src/c++98/.libs/libc++98convenience.a" \
  -L"$nt9x_tools/i386-crt/lib32" -lmingw32 -lmoldname -lmingwex -lcrtdll \
  "$nt9x_tools/i386-libgcc/libgcc.a" \
  -lkernel32 -luser32 -ladvapi32 -lshell32 -luuid -Wl,--end-group \
  "$nt9x_tools/i386-libgcc/crtend.o"
