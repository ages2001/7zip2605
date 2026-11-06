#!/usr/bin/env bash
set -euo pipefail
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_DICT_BUILD:-/tmp/7zip-win16-dictionary-stream-tests}
mkdir -p "$win16_build"
win16_units=()
for win16_unit in 7zDec 7zArcIn 7zBuf 7zStream 7zCrc Bcj2 Bra Bra86 BraIA64 Delta Lzma2Dec LzmaDec; do
 win16_units+=("$win16_source/C/$win16_unit.c")
done
win16_units+=("$win16_source/Compat/Win16/stream_folder.c" "$win16_source/Compat/Win16/deflate_stream.c")
if [[ ${WIN16_DICT_TARGET:-host} == dos ]]; then
 : "${WATCOM:?Set WATCOM to an extracted Open Watcom installation}"
 export PATH="$WATCOM/binl64:$PATH" INCLUDE="$WATCOM/h"
 cd "$win16_build"
 wcl -q -os -k8192 -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_WIN16_PAGED_DICT -dZ7_CRC_NUM_TABLES=1 -l=dos \
  -fe=DICSTRM.EXE "$win16_source/Compat/Win16/dictionary_stream_tests.c" "${win16_units[@]}"
else
 "${WIN16_HOST_CC:-cc}" -g -O1 -DZ7_WIN16 -DZ7_WIN16_PAGED_DICT -DZ7_CRC_NUM_TABLES=1 -fsanitize=address,undefined -fno-sanitize=alignment \
  "$win16_source/Compat/Win16/dictionary_stream_tests.c" "${win16_units[@]}" -o "$win16_build/dictionary-stream-tests"
fi
