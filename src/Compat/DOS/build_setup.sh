#!/usr/bin/env bash
set -euo pipefail
: "${WATCOM:?Set WATCOM to the Open Watcom installation}"
dos_source=$(cd "$(dirname "$0")/../.." && pwd)
dos_build=${DOS_SETUP_BUILD:-/workspace/nt9x-tests/dos32/setup}
mkdir -p "$dos_build"
export PATH="$WATCOM/binl64:$PATH"
export INCLUDE="$WATCOM/h"
cd "$dos_build"
for dos_kind in SETSTUB UNINST; do
 dos_flags=()
 if [[ $dos_kind == UNINST ]]; then dos_flags+=(-dZ7_DOS_UNINSTALL); fi
 wcl -q -os -k8192 -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_CRC_NUM_TABLES=1 \
  -dZ7_DOS_SETUPNAME='"7ZSETD32"' "${dos_flags[@]}" -l=dos -fe="$dos_kind.EXE" \
  "$dos_source/Compat/DOS/lifecycle.c" "$dos_source/C/7zCrc.c"
done
