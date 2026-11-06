# Verification of the 26.05 NT/9x distribution

Tests use disposable copies/overlays of the supplied VHDs. The original disk
images are not included in the distribution and were not used as writable
virtual drives. All application and fixture inputs have SHA-256 records.

## Systems

| System | Emulator / CPU | Main regression |
| --- | --- | --- |
| Windows NT 3.1 | QEMU TCG, Pentium III | PASS, candidate19 |
| Windows 95 RTM | QEMU TCG, Pentium III | PASS, candidate19 |
| Windows NT 4.0 SP1 | QEMU TCG, Pentium III | PASS, candidate19 |
| Windows NT 3.1 | Bochs, BX_CPU_LEVEL=3, FPU enabled | PASS, candidate20 |

Candidate20 uses exactly the same production payload and installer as
candidate19. Its GUI fixture allows five minutes for encryption on a slow CPU,
and its runner includes the separately tested help and link fixtures. The
previous 45-second fixture limit was too short for the Bochs encrypted GUI
operation; the operation completed later. That earlier timeout is not a pass.
Bochs' configured instruction rate is a test speed setting, not a performance
claim about physical 386 hardware. An FPU-less configuration was not tested.

QEMU OS tests are not i386 instruction-set tests. Bochs was compiled with CPU
level 3, so a newer instruction in an executed code path fails there. Runtime
libraries were also rebuilt for i386; only changing application compiler flags
would not suffice.

## Native fixtures

| Fixture | What it checks |
| --- | --- |
| GTEST | Exact binary round trips through 7z, ZIP, gzip, bzip2, xz and tar; multiple threads; staging and existing-archive updates on 8.3 filesystems; encryption; wrong passwords; malformed input; split volumes; drive-relative names and wildcards |
| GTEST --plugins | Additionally loads a real external hasher DLL and compares its digest with an independent FNV-1a computation |
| RTTEST | Exceptions/destructors, concurrent unwinding, software atomics, recursive locks, text/binary BSTR ownership and native automation interoperability where available |
| SETAPI | Setup's actual legacy file, registry, process and UI API adapters |
| SETTEST | Default installation, installed CLI/GUI launch, unrelated-shortcut protection, reinstall, uninstall and retention of a user-owned file |
| SETUNIC | NT4 installation outside the ANSI code page; exact UTF-16 Desktop/Programs targets; Unicode-path uninstall |
| COMPTEST | Actual 7zG compression/extraction, graphical and console SFX, Explorer DLL activation, real menu commands and Extract Here with exact output bytes |
| GUIROUND --advanced | Actual File Manager settings pages, encrypted headers, password prompt, create/extract dialogs, exact output bytes and normal process exit |
| GUIDROP | NT3 cross-process file drop into the actual File Manager, archive creation, CLI extraction and exact bytes |
| CTTEST / LVTEST | Common controls, 32-bit progress/spin values, buddy keyboard/veto/lifetime, images, toolbar resizing; list callbacks, sorting, selection, directional search, marquee selection and label editing |
| MNTEST / PHTEST / PSTEST | NT3 MENUEX menus, property pages and process-launch semantics |
| OLETEST | NT3 COM identity/lifetime, registered class activation, data formats, drag cancellation/errors and a separate receiving process |
| SHELLTST / ICONTEST | NT3 filesystem Shell operations, PIDLs, recovery, associations, image-list/icon ownership and registry icon refresh |
| MPRTEST | Resource information/parent queries against an injected provider tree, including ANSI/Unicode packing and required buffer sizes |
| METATEST | WIM security descriptors and alternate streams on FAT; explicit English failures, unchanged source archive and -snr recovery of stream bytes |
| LINKTEST | Explicit hard/symbolic-link failures on FAT, unchanged TAR metadata, intact ordinary file and archive integrity |
| GUIHELP | Embedded English topic index, actual topic content, full-text search, missing-topic feedback, clearing search and normal close |

Native comparison failures in the old Win95 progress control and NT4 spin
control led to application-local compatible classes. They are covered by the
final control and GUI reruns. Win95 delayed uninstall cleanup was also checked
across reboot; the scheduled temporary executable/directory was removed while
user-created installation files remained.

## Evidence and interpretation

The release source includes `Compat/NT9x/test-results/` with closed logs,
manifests, import audits and installed-file verification. A nonzero exit in a
wrong-password, malformed-input or unsupported-metadata test is intentional;
the fixture verifies the expected code and diagnostic. A successful process
launch alone is never treated as an archive round-trip pass.

The candidate CLI test copy was stripped separately from the installed CLI.
The PE linker timestamp and checksum differ; all other bytes and all PE
sections match. The manifests retain both actual hashes and the normalized
comparison. Later fixture assembly copies the payload CLI verbatim to avoid
this tool-generated metadata difference. Installed production files were
extracted from guest snapshots and compared byte-for-byte with the payload.

Import audits pass against the unmodified NT3.1 and Win95 system DLL exports
for all production executables, SFX modules, Explorer DLL and installer. These
checks complement actual guest tests: Win95 can export APIs that are only stubs.

These tests exercise the listed workflows and the full compiled codec
registry. They do not exhaust every input supported by every archive reader,
certify third-party plugins, or certify every network-server implementation.
The original MPR suite uses a controlled provider fixture; the live NT3.1
PCnet follow-up is described below. NT3.50 installer routing is covered as a
predicate, but that operating system was not boot-tested. NT3.51 SP5 was
subsequently boot-tested as described below. Filesystem limits
and unavailable metadata remain subject to the operating system as described
in the installed readme. The compatibility layer implements services needed
by this application, not arbitrary COM servers or the complete modern Shell.

## NT3.51 and setup-only follow-up (2026-10-22)

The supplied NT351.zip contains an installed NT3.51 SP5 tree. It was copied
into a FAT16 VHD with an NT boot sector and a boot.ini entry for WINNT351,
then booted under QEMU TCG (Pentium III, 64 MiB). The uploaded files and the
previous OS images were retained separately from writable test overlays.

The first short test exposed two issues, retained in nt351-initial evidence:

- NT3.51 Program Manager returned a CF_TEXT buffer with an embedded NUL and
  one padding byte after it. Setup incorrectly required NUL in the last byte,
  so reinstall/uninstall reported invalid data. Both ownership and empty-group
  checks now accept a terminator anywhere inside the bounded DDE buffer.
- The uploaded installation points TEMP/TMP to a missing directory. The
  uninstaller now creates a missing temporary directory hierarchy before
  making its temporary executable copy. Its installed executable is removed
  and user-created installation files remain intact.

Candidate22 changes only SETUP.EXE and Uninst.exe in the production package.
All other installed files are byte-identical to candidate19/candidate20.
The previous full regressions and Bochs i386 execution apply to those
unchanged application components. The new installer/uninstaller were rebuilt
with the same i386 toolchain; this setup-only update was not rerun in Bochs.

Fresh short setup runs cover NT3.1, NT3.51, Win95 RTM and NT4 SP1. Each checks
installation, default path/shortcut routing, actual CLI/GUI launch, unrelated
shortcut protection, reinstall, uninstall and user-file retention. All ten
installed production files were extracted from guest snapshots and compared
byte-for-byte with candidate22. Evidence is in test-results/setup22/.

NT3.51 additionally passed exact CLI archive round trips, real GUI archive
creation/extraction and embedded English help tests in 20.703 seconds for the
complete short suite. NT3.1's setup check took 38.970 seconds and NT4's took
9.604 seconds. Win95's final short setup run took 41.390 seconds. These are
emulator timings, not physical-hardware benchmarks.

On Win95, native GetTempPath selects the existing working directory when the
injected TEMP/TMP directory does not exist. The fixture records that selection
and checks the native fallback; NT tests exercise actual directory creation.
The first Win95 fixture's unconditional missing-directory assertion was a
fixture error, not an uninstall failure, and was corrected before the pass.


## Native file associations and final GUI revision

The Options / System page originally exposed a current-user association column.
NT3.x, NT4 and Windows 95 do not merge that Classes key into the shell's HKCR
view, so an apparently saved setting could fail to launch from the OS shell.
The legacy build now exposes one Program column and writes the shared HKCR
association. The unused second button is hidden and its keyboard/notification
routes cannot select an unsupported group.

Candidate23 changes only 7zFM.exe relative to candidate22; the standalone
installer embeds that new GUI with the nine byte-identical remaining files.
Its installer executable stub and uninstaller are unchanged from candidate22.
All build commands retain -march=i386. The original complete codec/component
regression and Bochs 386 execution remain evidence for unchanged components;
this association-only GUI revision has fresh QEMU tests on NT3.1, NT3.51 SP5,
Win95 RTM and NT4 SP1, rather than a claimed second complete Bochs run.

On all four guests the actual Options page selects the 7z archive type,
applies the association, launches a real archive through the OS's native
ShellExecuteA, confirms that the File Manager opens that archive, and removes
the association using Options again. A pre-existing 7-Zip association is
restored after the test. This tests the shell association launch path used by
file activation; it does not substitute the File Manager's internal archive
open command. The GUI regression also visits all six settings pages,
creates an encrypted archive with encrypted names, supplies its password,
extracts and compares exact bytes, and tests the embedded help viewer.

Reports and installed-file hashes are under test-results/final23/. All ten
installed files are compared with the final payload, including Uninst.exe.
The installer is separately extracted and all ten embedded files compared
byte-for-byte; no separately downloaded application runtime is required.
Test helpers are confined to source/test infrastructure and are not shipped
in bin/ or installed by SETUP.EXE.

## NT3.1 AMD PCnet live network follow-up

The guest uses its own NT3.1 TCP/IP and AM2100/AM1500T/PCnet driver from the
supplied NT3.1 installation media. No NT3.51 operating-system binaries were
copied into NT3.1. The QEMU PCI PCnet adapter requires bus mastering and an
edge-triggered IRQ 11 for this ISA-era native driver. A private option ROM
sets those hardware conditions before boot; it is test infrastructure, not
an application dependency. See network-test-host/README.md for reproduction.

The CLI smoke test maps S:, queries and enumerates the mapping, writes a
4096-byte file over UNC, creates/tests an archive with the installed CLI,
extracts through the mapped drive, compares every byte, and disconnects its
own mapping. The GUI test drives real Add to Archive and Copy dialogs over
UNC and compares the extracted bytes. Native ping replies were also checked.
The network target is a private loopback-only SMB1 share behind QEMU user
networking. The initial impacket backend needed AndX and missing-file fixes;
its share-enumeration RPC was incompatible with NT3.1. The GUI tests therefore
use Samba 4.22 with its actual SRVSVC RPC service. A native WNet enumeration
probe confirmed that the share could be enumerated on this backend.

The CLI pass used 16 MiB guest RAM and the GUI follow-up uses 64 MiB. GUI test
message/window timeouts allow old network-backed initialization to finish;
a timeout remains a failure and does not stand in for the byte comparisons.
Only sanitized logs and hashes are published; raw packet captures and SMB
server authentication logs are excluded. These tests cover the recorded
private share, not arbitrary domain/browser configurations or all SMB servers.

## About and NT3 Program Manager revision (candidate24)

The actual menu About dialog and complete setup/reinstall/uninstall sequence
passed on NT3.1, NT3.51 SP5, Windows 95 RTM and NT4 SP1. Reports and all ten
installed-file hashes are in `test-results/revision24/`. NT3 tests assert an
exactly-once File Manager and interactive Uninstall item, upgrade from the
older File Manager-only group, and removal of both owned items. The original
user-file preservation and missing temporary-directory tests also pass.

The GUI's non-resource PE section contents are byte-identical to candidate23;
only its About dialog resource changed. Prior association, archive and live
AMD PCnet network evidence remains in final23/ and network-nt31/ and was not
rerun for this resource change. Setup and Uninst.exe were rebuilt for i386
and exercised by the new native setup tests. These QEMU tests do not add an
NT3.50, Windows 98 or ME boot test or a new physical-386 execution claim.

OWN24 separately tests a user-edited uninstall command retaining the original
icon: setup refuses the conflict before modifying File Manager, removal keeps
the unrelated item/group, and the fixture restores the installed shortcuts.
The standalone installer was extracted independently and all ten embedded
files matched the distributed payload exactly. It needs no external payload.

## Visual and resource correction (candidate25)

This revision supersedes candidate24. Its About-only resource rebuild had
omitted the bundled GUI and manual resources; the earlier About/setup checks
did not cover that mistake. The corrected build uses gui-i386/combined.rc.
`audit_resources.py` compares all 169 resources with the previously tested
candidate23 and permits only the About dialog to differ. The GUI archive,
manual and association tests are rerun on the final executable.

The visual suite includes pixel checks for 32-to-16 scaling, small masked
bitmaps, transparent areas on a colored background with selection DC colors,
and preservation of the caller's DC colors. The actual File Manager toolbar
is also sampled: its icon corner must match COLOR_BTNFACE, not a white box.
NT3.51 uses the legacy extraction path because its ExtractIconExW export is
incomplete. Native ExtractIconEx return counts can include both requested
sizes; tests validate each separately owned handle instead of requiring one.

NT3 generic file/folder/computer/network/desktop/drive icons have original
VGA-compatible color artwork and explicit AND masks. Old native toolbars
receive bitmaps with the magenta key mapped to their button face. Those
bitmaps live until the toolbar is destroyed, and system-color changes rebuild
the toolbar. The original upstream bitmap files remain unchanged.

Reports and exact installed hashes are under test-results/visual25/. Only
7zFM.exe changes from candidate24; its tested setup/uninstaller and other
payload files remain byte-identical. The new standalone installer uses the
same setup stub with the updated embedded GUI. Extraction verifies all ten
payload files. No new 386 hardware/Bochs, NT3.50, Windows 98 or ME boot claim
is made by these QEMU checks. Win16 feasibility is documented separately in
Compat/Win16 and must not be mistaken for a complete Win16 application.

Visual evidence also samples actual bitmap-background pixels in the captured
native File Manager screenshots, comparing them with the neighboring toolbar
background. Coordinates and RGB values are recorded in
visual25/screenshots/pixels.json; this independently covers the white-box
regression in the resource-backed toolbar bitmap path.

The NT3.1 list-view fallback now implements LVCF_ORDER for column queries,
updates and insertion. Previously, saving a view could consume an uninitialized
order field and move the filename column behind other columns. LISTTEST checks
the individual column order against the order array, including an update that
must preserve a valid permutation. Existing saved layouts remain user settings;
the fix does not silently overwrite them.

NT3.1 returns failure for SPI_GETWORKAREA. Previously that left the screen-size
check with an uninitialized rectangle, hiding default toolbar labels and
selecting inappropriate small-screen dialog layouts. The A/W compatibility
entry points now return the primary screen bounds on NT3 only when the native
work-area query fails. Successful native results, including taskbar space on
later systems, remain authoritative. CTRLTEST checks valid A/W work-area
queries on NT; the actual GUI test checks room for labels below the icons.

The NT3.1 File Manager was also closed and reopened with a newly saved C:\
view. COLSAVE.EXE snapshots live widths through integer-only list-view messages
and records the saved registry values. The two snapshots are byte-identical:
all 14 stored properties remain, with seven visible columns (Name: 160 pixels;
the other six: 100 pixels). Screenshots show Name first and all seven toolbar
labels. Obsolete column settings were reset only in this disposable guest
before this test; the application does not erase existing user preferences.
See COLUMNS-FIRST.TXT, COLUMNS-SECOND.TXT and column_persistence in the manifest.
