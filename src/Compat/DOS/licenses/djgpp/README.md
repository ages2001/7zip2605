These unmodified notices are from DJGPP 2.05, tag `v2_05`, commit
`15a1b7e1eb3d0502a0d1956d3b3ad544f56f464b`:

https://github.com/jwt27/djgpp-cvs/tree/v2_05

`COPYING.DJ` describes the library license and distribution exceptions for
DJGPP runtime objects, including `libemu.a`. The DOS executable statically
links the x87 emulator. Its source and build instructions are available at
the pinned source tree above and in `Compat/DOS/build_emulator.sh`.

`CTIME.txt` retains the notices from `src/libc/ansi/time/ctime.c` in that
revision. `build_time.sh` builds a private copy with the GMT fallback offset
initialized to zero. It does not change the external source tree. Both the
original source and this modification must accompany the eventual source
distribution. The installer appends these notices to `DJGPP.TXT`.

The tested cross compiler is build-djgpp v3.4 (GCC 12.2.0):
https://github.com/andrewwutw/build-djgpp/releases/tag/v3.4
