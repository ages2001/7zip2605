#!/usr/bin/env bash
set -euo pipefail
: "${WATCOM:?Set WATCOM to Open Watcom}"
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_HASH_BUILD:-/workspace/nt9x-tests/win16/hash49/probe}
mkdir -p "$win16_build"
export PATH="$WATCOM/binl64:$PATH" INCLUDE="$WATCOM/h"
cd "$win16_build"
win16_main=${WIN16_HASH_MAIN:-hash_probe}
win16_exe=${WIN16_HASH_EXE:-HASH16.EXE}
win16_backend=("$win16_source/Compat/Win16/hash_file.c")
if [[ $win16_main == hash_failure_tests ]]; then win16_backend=(); fi
win16_units=()
for win16_unit in 7zCrc XzCrc64 Md5 Sha1 Sha256 Sha512 Sha3 Blake2s Xxh64; do
 win16_units+=("$win16_source/C/$win16_unit.c")
done
wcl -q -os -k16384 -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_ST \
 -dZ7_CRC_NUM_TABLES=1 -dZ7_CRC64_NUM_TABLES=1 -l=dos -fe="$win16_exe" \
 "$win16_source/Compat/Win16/$win16_main.c" "${win16_backend[@]}" "${win16_units[@]}"
