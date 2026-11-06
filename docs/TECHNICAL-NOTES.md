# Implementation & testing notes

Technical overview of the 7-Zip 26.05 legacy port (v1.0): how each target is
built, what it supports, and how it was verified. All testing was done on real
guest operating systems under QEMU, using private clones of the guest disks
(originals never modified).

## Targets and engine strategy

| Target | Engine | Toolchain |
|--------|--------|-----------|
| win16 (`7ZFM16.EXE`), dos16 (`7Z16C.EXE`) | from-scratch native 16-bit 7z/ZIP engine for the 8086 | Open Watcom, large model, `-0` (8086) |
| dos32 (`7Z.EXE`) | full upstream 7-Zip (Alone2) | DJGPP + CWSDPMI (bound in) |
| win32 (`7zsetw32.exe`) | full upstream 7-Zip (FM/GUI/CLI) | MinGW-w64 cross (see `src/Compat/NT9x/BUILD.md`) |

The native 16-bit objects are compiled for the 8086 ISA (no 186+/286+ opcodes),
confirmed by an instruction audit, so they are valid on an original 8088/8086.

## Native 16-bit edition (win16 + dos16)

- **Formats.** 7z and ZIP (Copy/Deflate/Deflate64) create, list, test, extract.
  gzip (RFC 1952), tar (POSIX ustar, store) and xz create + read. xz read uses
  the LZMA2 decoder; the uncompressed size comes from the xz index.
- **7zAES-256.** Reading password-protected 7z (method `06F10701`, AES-256 +
  SHA-256 key derivation) works on both the GUI and the CLI (`-pPASSWORD`).
  Wrong passwords fail gracefully. Verified byte-exact against desktop 7-Zip and
  cross-checked on the host against a Python/openssl reference.
- **Protected-mode write features.** In Standard / 386 Enhanced mode the GUI
  also creates xz archives and AES-256-encrypted 7z (method forced to LZMA,
  which AES wraps). These are not enabled in the real-mode CLI, where the
  low-memory LZMA encoder runs close to a performance limit; protected mode uses
  extended RAM and has no such limit. All four GUI write features (plain 7z,
  AES-256 7z, gzip, tar, xz) were created inside Windows 3.1 and verified
  byte-exact against the reference tools.
- **Report-view file list.** The file list is an owner-drawn report view with
  per-row file/folder icons (drawn with GDI primitives, no bitmap resources) and
  aligned Name / Size / Modified / Attr columns under a column-header bar
  (a dedicated window class, since Windows 3.x has no Header common control).
- **Wildcards and limits.** The CLI `a`/`h` commands expand on-disk wildcards
  (`*`, `?`, DOS 8.3 folded); the per-operation entry limit is 4096.
- **Self-tests.** The GUI ships an internal browser test suite (`/BROWSETEST`)
  that exercises directory and archive browsing, open/extract/test, refreshes
  and cleanup; it reports PASS with the shipped binary.

## 32-bit DOS edition (dos32)

`7Z.EXE` is the full upstream Alone2 CLI with every built-in format and codec
(including PPMd and large dictionaries), statically linked with an x87 emulator
and a deterministic ctime fallback, and with an unmodified CWSDPMI stub bound in
so no external `CWSDPMI.EXE` is required. Verified on a real DOS guest: create,
test and extract a 7z archive byte-exact, and the archive opens in modern 7-Zip.

## NT / 9x / Win32s edition (win32)

The upstream 7-Zip file manager, GUI and CLI built for old NT and 9x, plus a
Win32s build. Notable port work:

- **Typed registry adapter** — stores REG_DWORD / REG_BINARY / REG_MULTI_SZ and
  typed data over Win32s' REG_SZ-only REG.DAT with correct Win32 status/sizes,
  so the file manager persists its settings.
- **Logical-drive adapter** — rebuilds `GetLogicalDriveStrings{A,W}` (missing on
  Win32s) so the drive view works.
- **File-type association adapter** — Win32s ships non-functional Unicode shell
  and registry stubs (`RegOpenKeyExW`/`RegQueryValueExW`, `SHGetFileInfoW`,
  `ExtractIconExW`, `ExpandEnvironmentStringsW` fail with error 120/87), so the
  port resolves a registered type's name and icon through the ANSI entry points
  and its own PE/NE resource reader. Associating `.7z`/`.zip` with 7-Zip writes
  to and reads back from the typed registry on Win32s; full per-type icon parity
  in the file-manager list is not claimed.
- **Single combined installer** — one payload carries the NT and the Win32s file
  sets and selects the right one at run time by platform, skipping the shell
  extension on Win32s. Install + file manager + uninstall were verified on
  NT 3.1, NT 3.51, NT 4 SP1, Windows 95 and Win32s; the installed payload hashes
  match the embedded manifest on every OS (`verification/regression-matrix.txt`).
- **Uninstaller** — on Win32s the uninstaller runs in place (one task) to avoid
  a shell-hook hang; NT/9x keep the upstream temporary-copy path.

## Known limitations

- **PPMd and very large dictionaries** are not available on the 8086/real-mode
  target (real mode has ~640 KiB and a 16-bit paging window). Use the 32-bit DOS
  edition for the full engine.
- **xz creation and AES-256 encryption** are not enabled in the real-mode CLI
  (they ship in the protected-mode GUI); enabling them in the production CLI
  needs the real-mode encoder's layout sensitivity resolved.
- **Win32s uninstaller self-delete** is best-effort: a loaded Win32s module
  cannot delete itself and Windows 3.1 has no delayed-delete mechanism, so the
  uninstaller image and its directory may remain after an otherwise complete
  uninstall. This is cosmetic residue, never a hang.
- RAR, ZipAES (WinZip AE), BZip2 and ZIP64 are not implemented in the native
  16-bit engine (they are available via the 32-bit DOS and NT editions where the
  upstream engine provides them).
