#!/usr/bin/env bash
set -euo pipefail
: "${DJGPP_ROOT:?Set DJGPP_ROOT to the cross compiler installation}"
: "${DJGPP_SOURCE:?Set DJGPP_SOURCE to the DJGPP 2.05 source tree}"
dos_source=$(cd "$(dirname "$0")/../.." && pwd)
dos_build=${DOS_BUILD:-/workspace/nt9x-tests/dos32/build}
mkdir -p "$dos_build"
export DOS_EMU_BUILD="$dos_build/emu" DOS_TIME_BUILD="$dos_build/time"
bash "$dos_source/Compat/DOS/build_emulator.sh"
bash "$dos_source/Compat/DOS/build_time.sh"
"$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-gcc" -O2 -march=i386 -mtune=i386 -c "$dos_source/Compat/DOS/runtime.c" -o "$dos_build/dos_runtime.o"
exec make -C "$dos_source/CPP/7zip/Bundles/Alone2" -f makefile.gcc -j"${DOS_JOBS:-4}" \
 O="$dos_build" PROG=7Z.EXE ST_MODE=1 \
 CC="$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-gcc" \
 CXX="$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-g++" \
 CFLAGS_BASE="-O2 -c -march=i386 -mtune=i386 -DZ7_ST -DZ7_AFFINITY_DISABLE -include $dos_source/Compat/DOS/runtime.h" \
 LIB2="$dos_build/dos_runtime.o $DOS_TIME_BUILD/ctime.o $DOS_EMU_BUILD/libemu.a" \
 EXTRA_LINK_DEPS="$dos_build/dos_runtime.o $DOS_TIME_BUILD/ctime.o $DOS_EMU_BUILD/libemu.a" LFLAGS_NOEXECSTACK=
