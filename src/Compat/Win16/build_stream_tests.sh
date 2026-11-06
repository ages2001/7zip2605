#!/usr/bin/env bash
set -euo pipefail
: "${WATCOM:?Set WATCOM to an extracted Open Watcom installation}"
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_BUILD:-/workspace/nt9x-tests/win16/stream}
mkdir -p "$win16_build"
export PATH="$WATCOM/binl64:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
cd "$win16_build"
win16_units=()
for win16_unit in 7zDec 7zArcIn 7zBuf 7zStream 7zCrc Bcj2 Bra Bra86 BraIA64 Delta Lzma2Dec LzmaDec; do
 win16_units+=("$win16_source/C/$win16_unit.c")
done
wcl -q -bt=windows -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 \
 -dZ7_CRC_NUM_TABLES=1 -l=windows -fe=STREAM16.EXE \
 "$win16_source/Compat/Win16/stream_tests.c" "$win16_source/Compat/Win16/stream_folder.c" "$win16_source/Compat/Win16/deflate_stream.c" "$win16_source/Compat/Win16/paged_dictionary.c" "${win16_units[@]}"
