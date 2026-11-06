#!/usr/bin/env bash
set -euo pipefail
: "${WATCOM:?Set WATCOM to an extracted Open Watcom installation}"
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_BUILD:-/workspace/nt9x-tests/win16/console}
win16_main=${WIN16_MAIN:-console}
win16_exe=${WIN16_EXE:-7Z16C.EXE}
mkdir -p "$win16_build"
read -r -a win16_extra_flags <<< "${WIN16_CONSOLE_FLAGS:-}"
export PATH="$WATCOM/binl64:$PATH"
export INCLUDE="$WATCOM/h"
cd "$win16_build"
win16_units=()
for win16_unit in 7zDec 7zArcIn 7zBuf 7zStream 7zCrc Bcj2 Bra Bra86 BraIA64 Delta Lzma2Dec LzmaDec Aes XzCrc64 Md5 Sha1 Sha256 Sha512 Sha3 Blake2s Xxh64; do
 win16_units+=("$win16_source/C/$win16_unit.c")
done
wcl -q -os -k16384 -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_WIN16_PAGED_DICT -dZ7_ST \
 -dZ7_CRC_NUM_TABLES=1 -dZ7_CRC64_NUM_TABLES=1 "${win16_extra_flags[@]}" -l=dos -fe="$win16_exe" \
 "$win16_source/Compat/Win16/$win16_main.c" "$win16_source/Compat/Win16/hash_file.c" "$win16_source/Compat/Win16/paged_dictionary.c" "$win16_source/Compat/Win16/stream_folder.c" "$win16_source/Compat/Win16/deflate_stream.c" "$win16_source/Compat/Win16/deflate_encode.c" "$win16_source/Compat/Win16/zip_archive.c" \
 "$win16_source/Compat/Win16/lowmem_match_finder.c" "$win16_source/C/LzmaEnc.c" "${win16_units[@]}"
