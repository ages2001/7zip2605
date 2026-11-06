#!/usr/bin/env bash
# Independent host checks; native Windows/DOS runs remain separate.
set -euo pipefail
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_HOST_BUILD:-/tmp/7zip-win16-deflate-tests}
win16_cc=${WIN16_HOST_CC:-cc}
mkdir -p "$win16_build"
win16_flags=(-g -O1 -fsanitize=address,undefined -DZ7_ST -DZ7_CRC_NUM_TABLES=1)
win16_units=("$win16_source/Compat/Win16/deflate_stream.c" "$win16_source/C/7zCrc.c" "$win16_source/C/CpuArch.c")
"$win16_cc" "${win16_flags[@]}" "$win16_source/Compat/Win16/deflate_probe.c" \
 "$win16_source/Compat/Win16/paged_dictionary.c" "${win16_units[@]}" -o "$win16_build/deflate_probe"
# This fault probe includes paged_dictionary.c with injected I/O failures.
"$win16_cc" "${win16_flags[@]}" "$win16_source/Compat/Win16/deflate_cache_tests.c" \
 "${win16_units[@]}" -o "$win16_build/deflate_cache_tests"
python3 "$win16_source/Compat/Win16/deflate_host_tests.py" "$win16_build/deflate_probe"
python3 "$win16_source/Compat/Win16/deflate_host_tests.py" "$win16_build/deflate_probe" --paged
win16_fixture=$(mktemp -d "$win16_build/cache.XXXXXX")
python3 "$win16_source/Compat/Win16/generate_deflate_cache_fixture.py" --output "$win16_fixture/data"
TEMP="$win16_fixture/data" "$win16_build/deflate_cache_tests" "$win16_fixture/data/D81IN.DEF" "$win16_fixture/data/D81OUT.BIN"
