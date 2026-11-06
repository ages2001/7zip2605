#!/usr/bin/env bash
# Independent host checks complement native DOS/Windows execution.
set -euo pipefail
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_HOST_BUILD:-/tmp/7zip-win16-hash-tests}
win16_cc=${WIN16_HOST_CC:-cc}
mkdir -p "$win16_build"
win16_units=()
for win16_unit in 7zCrc XzCrc64 Md5 Sha1 Sha1Opt Sha256 Sha256Opt Sha512 Sha512Opt Sha3 Blake2s Xxh64 CpuArch; do
 win16_units+=("$win16_source/C/$win16_unit.c")
done
for win16_main in hash_probe hash_failure_tests; do
 win16_backend=()
 if [[ $win16_main == hash_probe ]]; then win16_backend+=("$win16_source/Compat/Win16/hash_file.c"); fi
 "$win16_cc" -g -O1 -fsanitize=address,undefined -fno-sanitize=alignment \
  -DZ7_WIN16 -DZ7_ST -DZ7_CRC_NUM_TABLES=1 -DZ7_CRC64_NUM_TABLES=1 \
  "$win16_source/Compat/Win16/$win16_main.c" "${win16_backend[@]}" \
  "${win16_units[@]}" -o "$win16_build/$win16_main"
done
