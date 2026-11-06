#!/usr/bin/env bash
set -euo pipefail
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_FS_BUILD:-/tmp/7zip-win16-filesystem-tests}
mkdir -p "$win16_build"
if [[ ${WIN16_FS_TARGET:-host} == dos ]]; then
 : "${WATCOM:?Set WATCOM to an extracted Open Watcom installation}"
 export PATH="$WATCOM/binl64:$PATH" INCLUDE="$WATCOM/h"
 cd "$win16_build"
 wcl -q -os -k8192 -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_ST -dZ7_CRC_NUM_TABLES=1 -l=dos \
  -fe=FS64TEST.EXE "$win16_source/Compat/Win16/filesystem_failure_tests.c" "$win16_source/C/7zCrc.c"
else
 "${WIN16_HOST_CC:-cc}" -g -O1 -DZ7_WIN16 -DZ7_ST -DZ7_CRC_NUM_TABLES=1 -fsanitize=address,undefined \
  "$win16_source/Compat/Win16/filesystem_failure_tests.c" "$win16_source/C/7zCrc.c" -o "$win16_build/filesystem-tests"
fi
