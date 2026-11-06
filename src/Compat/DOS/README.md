# 7-Zip 26.05 - 32-bit DOS edition (DJGPP)

The full 7-Zip 26.05 Alone2 command-line tool built for 32-bit protected-mode
DOS with DJGPP, with a CWSDPMI DPMI host bound into the executable (no external
CWSDPMI.EXE is needed). It requires a 386 or later with extended memory; Windows
is not required. All built-in 7-Zip archive formats and codecs are supported,
including PPMd and large LZMA/LZMA2 dictionaries. Part of the 7-Zip legacy port
by ages2001 - port version 1.0.

## Command-line usage

    7Z a archive.7z file...     add / create
    7Z l | t archive            list / test
    7Z x | e archive            extract with / without paths

Run `7Z` with no arguments for the full command and switch list.

## License

7-Zip is Copyright (C) 1999-2026 Igor Pavlov and is free software under the GNU
LGPL; see `LICENSE.TXT`. DJGPP and CWSDPMI are distributed under their own
licenses (`DJGPP.TXT`, `DJLIB.TXT`, `DJGPL.TXT`, `CWSLIC.TXT`, `COPYING.TXT`).
Unicode case data is covered by `UNICODE.TXT`. This is an independent community
port, not affiliated with or endorsed by the 7-Zip project or Igor Pavlov.
