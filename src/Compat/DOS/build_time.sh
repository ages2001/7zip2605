#!/usr/bin/env bash
# Build DJGPP 2.05 time conversion with a deterministic GMT fallback.
set -euo pipefail
: "${DJGPP_ROOT:?Set DJGPP_ROOT to the cross compiler installation}"
: "${DJGPP_SOURCE:?Set DJGPP_SOURCE to the DJGPP 2.05 source tree}"
dos_time_build=${DOS_TIME_BUILD:-${DOS_BUILD:-/workspace/nt9x-tests/dos32/build}/time}
mkdir -p "$dos_time_build"
# Keep the external source tree unchanged and retain all copyright notices.
python3 - "$DJGPP_SOURCE/src/libc/ansi/time/ctime.c" "$dos_time_build/ctime.c" <<'PY'
import sys
from pathlib import Path
source = Path(sys.argv[1]).read_text()
old = '  int_fast32_t stdoffset;'
if source.count(old) != 1:
    raise SystemExit('Expected the DJGPP 2.05 tzparse definition')
source = source.replace(old,
    '  int_fast32_t stdoffset = 0; /* GMT fallback has no parsed offset. */')
# Removing TZ is a change too: do not reuse a previously configured offset.
old = 'lcl_is_set > 0 && (name == NULL || stricmp(name, lcl_TZname) == 0)'
if source.count(old) != 1:
    raise SystemExit('Expected the DJGPP 2.05 tzset cache check')
source = source.replace(old,
    'lcl_is_set > 0 && name != NULL && stricmp(name, lcl_TZname) == 0')
Path(sys.argv[2]).write_text(source)
PY
"$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-gcc" \
 -O2 -march=i386 -mtune=i386 -fno-strict-aliasing \
 -I"$DJGPP_SOURCE/include" -I"$DJGPP_SOURCE/src/libc/ansi/time" \
 -c "$dos_time_build/ctime.c" -o "$dos_time_build/ctime.o"
