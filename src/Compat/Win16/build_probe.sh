#!/usr/bin/env bash
set -euo pipefail
: "${WATCOM:?Set WATCOM to an extracted Open Watcom installation}"
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_BUILD:-/workspace/nt9x-tests/win16/probe}
mkdir -p "$win16_build"
export PATH="$WATCOM/binl64:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
cd "$win16_build"
wcl -q -bt=windows -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -l=windows \
 -fe=DEC16.EXE "$win16_source/Compat/Win16/decoder_probe.c" "$win16_source/C/LzmaDec.c"
