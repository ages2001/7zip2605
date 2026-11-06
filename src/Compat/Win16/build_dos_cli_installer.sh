#!/usr/bin/env bash
# Build the dos16 installer: a 16-bit real-mode DOS self-extractor that installs
# the native 16-bit DOS CLI (7Z16C.EXE). Reuses the shared DOS installer stub
# (Compat/DOS/lifecycle.c), built here with the completion message pointing at
# 7Z16C.EXE instead of the 32-bit 7Z.EXE.
set -euo pipefail
: "${WATCOM:?Set WATCOM to the Open Watcom installation}"
: "${WIN16_CLI_EXE:?Set WIN16_CLI_EXE to the tested native 16-bit DOS CLI (7Z16C.EXE)}"
src=$(cd "$(dirname "$0")/../.." && pwd)
build=${DOS16_SETUP_BUILD:-/workspace/nt9x-tests/dos16/setup}
mkdir -p "$build"
export PATH="$WATCOM/binl64:$PATH" INCLUDE="$WATCOM/h"
cd "$build"
for kind in SETSTUB UNINST; do
 flags=()
 if [[ $kind == UNINST ]]; then flags+=(-dZ7_DOS_UNINSTALL); fi
 wcl -q -os -k8192 -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_CRC_NUM_TABLES=1 \
  -dZ7_DOS_MAINEXE='"7Z16C.EXE"' -dZ7_DOS_SETUPNAME='"7ZSETD16"' "${flags[@]}" -l=dos -fe="$kind.EXE" \
  "$src/Compat/DOS/lifecycle.c" "$src/C/7zCrc.c"
done
payload=$(mktemp -d "$build/payload.XXXXXX")
trap 'rm -rf "$payload"' EXIT
cp "$WIN16_CLI_EXE" "$payload/7Z16C.EXE"
cp "$src/DOC/License.txt" "$payload/LICENSE.TXT"
cp "$src/Compat/Win16/README.md" "$payload/README.TXT"
cp UNINST.EXE "$payload/UNINST.EXE"
python3 "$src/Compat/DOS/pack_setup.py" --stub SETSTUB.EXE \
 --payload "$payload" --output "$build/7zsetd16.exe"
