#!/usr/bin/env bash
# Native-host sanitizers complement, but do not replace, DOS/Windows tests.
set -euo pipefail
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_HOST_BUILD:-/tmp/7zip-win16-extract-tests}
win16_cc=${WIN16_HOST_CC:-cc}
mkdir -p "$win16_build"
win16_units=()
for win16_unit in 7zDec 7zArcIn 7zBuf 7zStream 7zCrc Bcj2 Bra Bra86 BraIA64 Delta Lzma2Dec LzmaDec XzCrc64 Md5 Sha1 Sha256 Sha512 Sha3 Blake2s Xxh64 Sha1Opt Sha256Opt Sha512Opt CpuArch; do
 win16_units+=("$win16_source/C/$win16_unit.c")
done
for win16_main in ${WIN16_HOST_MAINS:-console extract_failure_tests zip_failure_tests rename_failure_tests update_failure_tests zip_create_failure_tests zip_update_failure_tests tree_failure_tests}; do
 "$win16_cc" -g -O1 -fsanitize=address,undefined -fno-sanitize=alignment \
  -DZ7_CRC_NUM_TABLES=1 -DZ7_CRC64_NUM_TABLES=1 -DZ7_WIN16 -DZ7_WIN16_PAGED_DICT -DZ7_ST "$win16_source/Compat/Win16/$win16_main.c" \
  "$win16_source/Compat/Win16/hash_file.c" "$win16_source/Compat/Win16/paged_dictionary.c" "$win16_source/Compat/Win16/stream_folder.c" "$win16_source/Compat/Win16/deflate_stream.c" "$win16_source/Compat/Win16/deflate_encode.c" "$win16_source/Compat/Win16/zip_archive.c" "$win16_source/Compat/Win16/lowmem_match_finder.c" \
  "$win16_source/C/LzmaEnc.c" "${win16_units[@]}" \
  -o "$win16_build/$win16_main"
done
