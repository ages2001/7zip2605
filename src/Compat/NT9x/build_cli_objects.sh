#!/usr/bin/env bash
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
nt9x_flags='-MMD -MP -march=i386 -mcrtdll=crtdll -DZ7_LEGACY_WIN32 -DZ7_NO_UNICODE -DZ7_EXTERNAL_CODECS -DEXPORT_CODECS -DZ7_WIN32_WINNT_MIN=0x030A -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -DUSE_THREADS_CreateThread'
if [[ -n ${NT9X_EXTRA_CFLAGS:-} ]]; then
  nt9x_flags+=" $NT9X_EXTRA_CFLAGS"
fi
mkdir -p "$nt9x_build/cli-i386"
nt9x_stamp="$nt9x_build/cli-i386/flags.txt"
if [[ ! -f "$nt9x_stamp" ]] || [[ $(cat "$nt9x_stamp") != "$nt9x_flags" ]]; then
  printf '%s\n' "$nt9x_flags" > "$nt9x_stamp"
fi
cd "$nt9x_source/CPP/7zip/Bundles/Alone2"
make -f makefile.gcc -f "$nt9x_source/Compat/NT9x/cli_objects.mk" \
  -j"${NT9X_JOBS:-4}" nt9x_objects IS_MINGW=1 O="$nt9x_build/cli-i386" \
  CC="$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32" \
  CXX="$nt9x_tools/root/usr/bin/i686-w64-mingw32-g++-win32" \
  RC="$nt9x_tools/root/usr/bin/i686-w64-mingw32-windres" \
  RFLAGS="--preprocessor=$nt9x_tools/root/usr/bin/i686-w64-mingw32-gcc-win32 --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I $nt9x_tools/include-aliases" \
  CFLAGS_BASE2="$nt9x_flags" CXXFLAGS_EXTRA="$nt9x_flags" \
  NT9X_RESPONSE="$nt9x_build/i386-objects.rsp" NT9X_FLAGS_STAMP="$nt9x_stamp"

"$nt9x_tools/root/usr/bin/i686-w64-mingw32-g++-win32" $nt9x_flags -O2 -c "$nt9x_source/CPP/7zip/Compress/CodecExports.cpp" -o "$nt9x_build/cli-i386/CodecExports.o"
printf '%s\n' "$nt9x_build/cli-i386/CodecExports.o" >> "$nt9x_build/i386-objects.rsp"
