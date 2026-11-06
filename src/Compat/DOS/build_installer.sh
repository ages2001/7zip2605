#!/usr/bin/env bash
# Build a self-contained DOS installer from the bound DOS CLI.
set -euo pipefail
: "${WATCOM:?Set WATCOM to the Open Watcom installation}"
: "${DOS_BOUND_EXE:?Set DOS_BOUND_EXE to the standalone DOS 7Z.EXE}"
: "${CWSDPMI_SOURCE:?Set CWSDPMI_SOURCE to the licensed CWSDPMI source tree}"
dos_source=$(cd "$(dirname "$0")/../.." && pwd)
export DOS_SETUP_BUILD=${DOS_SETUP_BUILD:-/workspace/nt9x-tests/dos32/setup}
mkdir -p "$DOS_SETUP_BUILD"
dos_payload=$(mktemp -d "$DOS_SETUP_BUILD/payload.XXXXXX")
trap 'rm -rf "$dos_payload"' EXIT
cp "$DOS_BOUND_EXE" "$dos_payload/7Z.EXE"
cp "$dos_source/DOC/License.txt" "$dos_payload/LICENSE.TXT"
cp "$dos_source/Compat/DOS/README.md" "$dos_payload/README.TXT"
cp "$dos_source/Compat/DOS/UNICODE-LICENSE.txt" "$dos_payload/UNICODE.TXT"
cp "$CWSDPMI_SOURCE/src/COPYING.CWS" "$dos_payload/CWSLIC.TXT"
cp "$CWSDPMI_SOURCE/src/COPYING" "$dos_payload/COPYING.TXT"
cp "$dos_source/Compat/DOS/licenses/djgpp/COPYING.DJ" "$dos_payload/DJGPP.TXT"
cat "$dos_source/Compat/DOS/licenses/djgpp/CTIME.txt" >> "$dos_payload/DJGPP.TXT"
cp "$dos_source/Compat/DOS/licenses/djgpp/COPYING.LIB" "$dos_payload/DJLIB.TXT"
cp "$dos_source/Compat/DOS/licenses/djgpp/COPYING" "$dos_payload/DJGPL.TXT"
bash "$dos_source/Compat/DOS/build_setup.sh"
cp "$DOS_SETUP_BUILD/UNINST.EXE" "$dos_payload/UNINST.EXE"
python3 "$dos_source/Compat/DOS/pack_setup.py" \
  --stub "$DOS_SETUP_BUILD/SETSTUB.EXE" --payload "$dos_payload" \
  --output "$DOS_SETUP_BUILD/7zsetd32.exe"
