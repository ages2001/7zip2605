# 7-Zip 26.05 - native 16-bit edition (Windows 3.0/3.1x and DOS)

A native 16-bit build of 7-Zip 26.05 for Windows 3.0/3.1x and real-mode DOS,
compiled with Open Watcom for the 8086/8088 (large model, no 186+ opcodes).
Part of the 7-Zip legacy port by ages2001 - port version 1.0.

## Components

- `7ZFM16.EXE` - Windows 3.0/3.1x file manager (GUI). Browse the filesystem and
  7z/ZIP archives in a report view with icons and Name/Size/Modified/Attr
  columns; create, extract and test archives; file checksums and associations.
  Running in Standard or 386 Enhanced mode it also creates gzip, tar and xz
  archives and AES-256 encrypted 7z archives.
- `7Z16C.EXE` - real-mode DOS command-line tool. Create 7z/ZIP/gzip/tar, list,
  test and extract; read xz; read AES-256 encrypted 7z (`-pPASSWORD`). Runs on
  any DOS, down to the original 8088/8086.

## Command-line usage

    7Z16C a archive.7z file...        add / create
    7Z16C a archive.zip -m0=deflate   create a ZIP
    7Z16C a archive.gz  file          gzip one file
    7Z16C a archive.tar file...       tar (store)
    7Z16C l | t archive               list / test
    7Z16C x | e archive dir           extract with / without paths

Paths use DOS 8.3 components; non-ASCII names are shown with `\uXXXX` escapes.

## Notes

- xz creation and AES-256 encryption run from the protected-mode GUI; they are
  not enabled in the real-mode CLI, where the low-memory LZMA encoder runs close
  to a performance limit.
- PPMd and very large dictionaries are not available on the 8086/real-mode
  target; use the 32-bit DOS edition for the full upstream engine.

## License

7-Zip is Copyright (C) 1999-2026 Igor Pavlov and is free software under the GNU
LGPL; see `LICENSE.TXT`. This is an independent community port, not affiliated
with or endorsed by the 7-Zip project or Igor Pavlov.
