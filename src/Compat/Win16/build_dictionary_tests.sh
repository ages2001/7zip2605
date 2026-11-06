#!/usr/bin/env bash
set -euo pipefail
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_DICT_BUILD:-/tmp/7zip-win16-dictionary-tests}
mkdir -p "$win16_build"
if [[ ${WIN16_DICT_TARGET:-host} == dos ]]; then
 : "${WATCOM:?Set WATCOM to an extracted Open Watcom installation}"
 export PATH="$WATCOM/binl64:$PATH" INCLUDE="$WATCOM/h"
 cd "$win16_build"
 wcl -q -os -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -l=dos \
  -fe=DICTEST.EXE "$win16_source/Compat/Win16/paged_dictionary_tests.c"
else
 "${WIN16_HOST_CC:-cc}" -g -O1 -fsanitize=address,undefined \
  "$win16_source/Compat/Win16/paged_dictionary_tests.c" -o "$win16_build/dictionary-tests"
 (cd "$win16_build" && ./dictionary-tests)
fi
