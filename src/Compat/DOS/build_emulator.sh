#!/usr/bin/env bash
# Rebuild DJGPP's optional static x87 emulator for the i386 target.
set -euo pipefail
: "${DJGPP_ROOT:?Set DJGPP_ROOT to the cross compiler installation}"
: "${DJGPP_SOURCE:?Set DJGPP_SOURCE to the DJGPP 2.05 source tree}"
dos_emu_build=${DOS_EMU_BUILD:-${DOS_BUILD:-/workspace/nt9x-tests/dos32/build}/emu}
mkdir -p "$dos_emu_build"
dos_emu_flags=(-O2 -march=i386 -mtune=i386)
"$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-g++" "${dos_emu_flags[@]}" \
 -std=gnu++98 -fno-exceptions -fno-strict-aliasing \
 -c "$DJGPP_SOURCE/src/libemu/src/emu387.cc" -o "$dos_emu_build/emu387.o"
"$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-gcc" "${dos_emu_flags[@]}" \
 -I"$DJGPP_SOURCE/include" -c "$DJGPP_SOURCE/src/libemu/src/npxsetup.c" \
 -o "$dos_emu_build/npxsetup.o"
"$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-gcc" "${dos_emu_flags[@]}" -DLIB=libemu \
 -c "$DJGPP_SOURCE/src/ident.c" -o "$dos_emu_build/id_emu.o"
# An archive is target-independent; the host ar avoids cross-binutils runtime
# dependencies. The DJGPP linker reads the contained COFF objects normally.
"${DOS_HOST_AR:-ar}" rcs "$dos_emu_build/libemu.a" \
 "$dos_emu_build/emu387.o" "$dos_emu_build/npxsetup.o" "$dos_emu_build/id_emu.o"
