#!/usr/bin/env bash
set -euo pipefail
: "${WATCOM:?Set WATCOM to Open Watcom}"
: "${WIN16_GUI_EXE:?Set WIN16_GUI_EXE to the tested native GUI}"
# WIN16_CLI_EXE is optional: set it to also bundle the real-mode DOS CLI
# (7Z16C.EXE) in the Windows installer. The DOS CLI ships on its own as the
# dos16 target, so the Windows installer is GUI-only when this is unset.
WIN16_CLI_EXE=${WIN16_CLI_EXE:-}
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_SETUP_BUILD:-/workspace/nt9x-tests/win16/setup33}
mkdir -p "$win16_build"
export PATH="$WATCOM/binl64:$PATH" INCLUDE="$WATCOM/h:$WATCOM/h/win"
cd "$win16_build"
for win16_kind in SETSTUB UNINST16; do
 win16_flags=()
 if [[ $win16_kind == UNINST16 ]]; then win16_flags+=(-dZ7_UNINSTALL); fi
 wcl -q -zu -os -bt=windows -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_CRC_NUM_TABLES=1 \
  "${win16_flags[@]}" -l=windows -fe="$win16_kind.EXE" \
  "$win16_source/Compat/Win16/setup.c" "$win16_source/C/7zCrc.c"
 (cd "$win16_source/Compat/Win16"
  wrc -q -30 -bt=windows -fo="$win16_build/$win16_kind.res" setup.rc "$win16_build/$win16_kind.EXE")
done
wcl -q -os -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_CRC_NUM_TABLES=1 \
 -l=dos -fe=UNINSTC.EXE "$win16_source/Compat/Win16/uninstall_files.c" "$win16_source/C/7zCrc.c"
win16_payload=$(mktemp -d "$win16_build/payload.XXXXXX")
trap 'rm -rf "$win16_payload"' EXIT
cp "$WIN16_GUI_EXE" "$win16_payload/7ZFM16.EXE"
if [[ -n $WIN16_CLI_EXE ]]; then cp "$WIN16_CLI_EXE" "$win16_payload/7Z16C.EXE"; fi
cp UNINST16.EXE UNINSTC.EXE "$win16_payload/"
cp "$win16_source/DOC/License.txt" "$win16_payload/LICENSE.TXT"
cp "$win16_source/Compat/Win16/README.md" "$win16_payload/README.TXT"
python3 "$win16_source/Compat/DOS/pack_setup.py" --stub SETSTUB.EXE \
 --payload "$win16_payload" --output "$win16_build/7zsetw16.exe"
