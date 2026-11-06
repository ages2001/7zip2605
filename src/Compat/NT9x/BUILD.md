# Building the 7-Zip 26.05 NT/9x port

The upstream base is commit `b88c6bc09339de32f7d18bbab900686b244f42ba` from
https://github.com/ip7z/7zip. This is an unofficial compatibility port.
Production source, build scripts, help and application messages are English.
The build and packaging scripts never commit, push or publish anything.

## Host and toolchain

Use Debian 13 amd64 with gcc/g++, make, binutils, git, Python 3 with venv,
dpkg-deb, tar, bison, flex, gawk, libgmp-dev, libmpfr6, libmpc3, libisl23,
zlib1g and libzstd1. The bootstrap extracts its cross tools privately; it does
not install packages globally. Network access is needed for initial downloads.
Choose paths without spaces for the toolchain and build directories.

```sh
export NT9X_TOOLS=/workspace/nt9x-tools
export NT9X_BUILD=/workspace/nt9x-build
export NT9X_JOBS=3
python3 Compat/NT9x/bootstrap_toolchain.py
bash Compat/NT9x/build_all.sh
```

`toolchain-lock.json` records SHA-256 hashes and public URLs for the Debian
cross compiler, source archives and official 26.05 installer used to obtain the
English manual. MinGW-w64 is checked out at the recorded commit. The bootstrap
verifies downloads, refuses a modified MinGW source checkout, extracts the
manual, and builds private i386 runtime libraries. `NT9X_DOWNLOAD_CACHE` can
point at a directory containing the exact locked Debian packages.
`NT9X_MINGW_SOURCE` can point at an existing clean MinGW Git clone.

The private runtime consists of the MinGW CRT, GCC libgcc, libsupc++, and the
C++98/C++11 libstdc++ archives, all compiled with `-march=i386`. The target uses
Windows' CRTDLL, not MSVCRT or UCRT. Do not replace these libraries with stock
i686 MinGW archives: doing so can silently introduce instructions newer than
i386. The production object builds track header and compiler-flag changes.
Do not mix Unicode, Z7_DEVICE_FILE or external-codec ABI settings across objects.

The complete toolchain and application were also built in a second private
directory from clean runtime sources. This establishes rebuildability; binary
identity across build paths or timestamps is not promised.

## Outputs and installer

`build_all.sh` builds these production files in `$NT9X_BUILD`:

| Build file | Distribution name |
| --- | --- |
| 7zz-i386.exe | 7z.exe |
| 7zFM-i386.exe | 7zFM.exe |
| 7zG.exe | 7zG.exe |
| 7-zip.dll | 7-zip.dll |
| 7z.sfx | 7z.sfx |
| 7zCon.sfx | 7zCon.sfx |
| 7zipUninstall-i386.exe | Uninst.exe |
| 7zipInstall-i386.exe | installer prefix |

The front ends contain the archive engines and support optional external
Codecs/Formats plugins. A separate 7z.dll is unnecessary. The GUI includes the
English manual in a native searchable viewer, without HTML Help dependencies.
`NT9X_MANUAL` can override the extracted manual HTML directory.

To assemble an installer and its test payload:

```sh
python3 Compat/NT9x/build_setup_fixture.py /workspace/nt9x-candidate
```

This strips executable symbols, adds readme/history/license notices, compresses
the payload, and appends it to the installer prefix. The small setup decoder
uses LZMA without executable filters; do not enable automatic BCJ filtering in
its payload. The result is `SETUP.EXE`, with installed files under `payload/`.
The accompanying SETAPI/SETTEST executables are test tools, not release files.

Installation defaults to C:\7-Zip on NT 3.x, with a Program Manager group.
Windows 95/NT4 use the registered ProgramFilesDir and create Desktop/Programs
shortcuts. The uninstaller retains unrelated files and shortcuts. Windows 95
schedules deletion of locked temporary files for reboot. NT4 paths and shell
links use Unicode APIs. NT3 Program Manager commands and Win95 paths must be
representable in the system ANSI code page.

## Tests

`build_all.sh` also builds the native fixtures by default. Set
`NT9X_BUILD_TESTS=0` to build production components only. Test programs must not
be included in distribution bin/. See TESTING.md for fixture roles, evidence
and the distinction between QEMU OS tests and Bochs i386 tests.
`build_association_tests.sh` builds ASSTEST and the targeted GUI runners;
`build_short_tests.sh` builds the setup/NT3.51 and live-network smoke tests.
These fixtures use disposable guest workspaces and must not be run against
an unrelated installation. Live PCnet host setup is documented in
network-test-host/README.md.

Import audits use the actual guest system DLLs and Python pefile:

```sh
"$NT9X_TOOLS/python/bin/python" Compat/NT9x/audit_imports.py \
  "$NT9X_BUILD/7zFM-i386.exe" /path/to/guest/system-dlls
```

An import audit does not detect Win95 stub exports or prove API behavior.
Always run the application in the target guest. Use copies/overlays of supplied
VHDs; never modify the originals. Save closed test logs and SHA-256 hashes of
exact inputs before another test overwrites a log.

`build_metadata_fixture.sh` additionally needs extracted wimtools/libwim,
ntfs-3g/libntfs-3g and libfuse3. It builds a disposable NTFS image and captures
a WIM with a security descriptor and alternate stream. It does not mount or
modify any supplied guest image.

## Distribution ZIP

After completing the guest tests and collecting matching manifests under
Compat/NT9x/test-results, package the verified candidate:

```sh
python3 Compat/NT9x/package_distribution.py /workspace/nt9x-candidate \
  /workspace/artifacts/7Z2605NX.ZIP
```

Packaging refuses failed/pending tests, mismatched installed payload hashes,
modified evidence, unexpected source binaries and an existing output archive.
It verifies every ZIP CRC and the SHA-256 manifest after writing. The ZIP has
bin/ and src/ directories. It contains no virtual disks, host toolchain binaries
or test executables. No repository publication is performed.

The About/Program Manager revision can be packaged with
`--revision-base /path/to/candidate23`. The packager first validates all prior
regression evidence against that base, then compares every non-resource GUI
PE section byte for byte (the unchanged relocation section may move). It also
requires new installation, reinstallation, uninstallation and About-dialog
reports, exact installed hashes on all four supplied OS images, and a fresh
standalone-installer payload check under `test-results/revision24/`.
`build_short_tests.sh` builds ABOUT24/REV24; `build_installer.sh` builds OWN24,
the NT3-only user-edited Program Manager item regression.

For candidate25, run `audit_resources.py` with pefile against the verified
candidate23 GUI, collect the visual25 evidence, then use:

```sh
python3 Compat/NT9x/package_visual_distribution.py /path/to/candidate25 \
  /path/to/candidate24 /path/to/candidate23 /path/to/output.zip
```

This validates the prior core/setup evidence, new actual-GUI pixel and
functional reports, resource coverage and identical setup stub before making
the ZIP. Always compile the combined resource file generated by
build_support.sh; compiling FileManager/resource.rc alone omits other dialogs
and the embedded manual.
