#!/usr/bin/env bash
# Native real-file/fault tests. Run LIFE77.EXE with a new private directory.
set -euo pipefail
: "${WATCOM:?Set WATCOM to Open Watcom}"
dos_source=$(cd "$(dirname "$0")/../.." && pwd)
dos_tests=${DOS_LIFECYCLE_TEST_BUILD:-/tmp/7zip-dos-lifecycle-tests}
mkdir -p "$dos_tests"
export PATH="$WATCOM/binl64:$PATH" INCLUDE="$WATCOM/h"
cd "$dos_tests"
wcl -q -os -k8192 -bt=dos -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 \
 -dZ7_CRC_NUM_TABLES=1 -l=dos -fe=LIFE77.EXE \
 "$dos_source/Compat/DOS/tests/lifecycle_tests.c" "$dos_source/C/7zCrc.c"
