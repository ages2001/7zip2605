#!/usr/bin/env bash
# Run host Deflate64 checks, or build the native DOS cache-failure probe.
set -euo pipefail
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_HOST_BUILD:-/tmp/7zip-win16-deflate64-tests}
mkdir -p "$win16_build"
if [[ ${WIN16_TEST_TARGET:-host} == dos ]]; then
 : "${WATCOM:?Set WATCOM to an extracted Open Watcom installation}"
 export PATH="$WATCOM/binl64:$PATH" INCLUDE="$WATCOM/h"
 cd "$win16_build"
 wcl -q -os -k16384 -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 \
  -dZ7_ST -dZ7_CRC_NUM_TABLES=1 -l=dos -fe=D64TEST.EXE \
  "$win16_source/Compat/Win16/deflate_cache_tests.c" \
  "$win16_source/Compat/Win16/deflate_stream.c" \
  "$win16_source/C/7zCrc.c"
else
 : "${SEVENZIP:?Set SEVENZIP to an independent 7-Zip executable}"
 win16_cc=${WIN16_HOST_CC:-cc}
 win16_flags=(-g -O1 -fsanitize=address,undefined -DZ7_ST -DZ7_CRC_NUM_TABLES=1)
 win16_units=("$win16_source/Compat/Win16/deflate_stream.c" "$win16_source/C/7zCrc.c" "$win16_source/C/CpuArch.c")
 "$win16_cc" "${win16_flags[@]}" "$win16_source/Compat/Win16/deflate_probe.c" \
  "$win16_source/Compat/Win16/paged_dictionary.c" "${win16_units[@]}" -o "$win16_build/deflate_probe"
 "$win16_cc" "${win16_flags[@]}" "$win16_source/Compat/Win16/deflate_cache_tests.c" \
  "${win16_units[@]}" -o "$win16_build/deflate_cache_tests"
 python3 "$win16_source/Compat/Win16/deflate64_host_tests.py" --probe "$win16_build/deflate_probe" --sevenzip "$SEVENZIP"
 win16_fixture=$(mktemp -d "$win16_build/cache.XXXXXX")
 python3 "$win16_source/Compat/Win16/generate_deflate64_fixture.py" --output "$win16_fixture/data" --sevenzip "$SEVENZIP"
 TEMP="$win16_fixture/data" "$win16_build/deflate_cache_tests" "$win16_fixture/data/MAX64.DEF" "$win16_fixture/data/MAX64.BIN" 64
fi
