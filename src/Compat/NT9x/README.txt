7-Zip 26.05 for Windows NT 3.x and Windows 95

This is an unofficial compatibility port of Igor Pavlov's 7-Zip 26.05.
It contains the original archive engines with an i386 build and additional
legacy Windows API implementations. It is not an official 7-Zip release.

Run 7zsetw32.exe to install. Windows NT 3.x uses C:\7-Zip and a Program Manager
7-Zip 26.05 group. Windows 95 and Windows NT 4 use the registered Program Files
directory and create Desktop and Programs shortcuts. 7zsetw32.exe /S performs a
silent installation; /D=C:\your-directory selects another destination.
Run Uninst.exe in the installation directory to uninstall. User-created files
in that directory are retained. Windows 95 can finish locked-file cleanup at
the next reboot.

7zFM.exe is the File Manager, 7z.exe is the standalone command-line program,
and 7zG.exe is the graphical command-line program. The archive engines are
built into these programs, so they do not require a separate 7z.dll. Compatible
optional codec/format DLLs can be loaded from Codecs and Formats directories;
those DLLs must themselves support the operating system and CPU.
7-zip.dll provides the Explorer context menu on Windows 95 and Windows NT 4.
7z.sfx and 7zCon.sfx are graphical and console self-extracting archive modules.

The English manual is built into the graphical programs. Use Help or F1;
no HTML Help installation is required. Source and rebuild instructions are in
the accompanying src directory under Compat/NT9x/BUILD.md.

The minimum instruction-set target is i386, including runtime libraries.
Memory and disk requirements depend on the selected compression method and
dictionary size. Choose a small dictionary on a machine with little memory.
Long filenames, Unicode filenames, permissions, links, alternate streams and
maximum file sizes also depend on the operating system and filesystem.
Unavailable metadata is reported in English. The original archive retains
that information; keep it to preserve metadata that cannot be applied.
Use -snr explicitly to recover alternate streams as ordinary files.

The NT 3.x compatibility Shell supplies filesystem browsing, file operations,
associations, icons and file-drop support. Files deleted with recovery enabled
are stored in 7ZTRASH on the same drive; the compatibility context menu can
restore the latest deleted item without overwriting an existing destination.
It does not install a modern Explorer shell or a general-purpose COM runtime.

The bundled 7zWF.dll adds file drops to the 32-bit Windows NT File Manager.
Drop onto a directory-list folder, its parent (..) row, a tree folder, or the open directory's
background. Ctrl copies and Shift moves. Extracted archive files remain alive
until the receiving operation finishes; moving these temporary files does not
delete entries from the source archive. Windows 95 uses native OLE for Win32
receivers; its separate 16-bit WinFile is not handled by this add-on.

See TESTING.md in src/Compat/NT9x for the exact tested systems and test limits.
License.txt contains upstream and runtime copyright and license notices.

Windows 3.1x + Microsoft Win32s 1.30c: the 32-bit application is supported.
7zsetw32.exe installs to C:\7-Zip with a Program Manager group; the File
Manager browses drives, creates/tests/extracts archives and persists its
settings, and the About box reads x86-32 / "and Win3.1x with Win32s". A typed
registry adapter (win32s_registry.c) and a logical-drive adapter
(win32s_system.c) provide the APIs Win32s lacks. The uninstaller runs in place
and does not hang; it removes the group, all installed files and the registry
entries and keeps user files. Because a loaded Win32s module cannot delete
itself and Windows 3.1 has no delayed-delete mechanism, the uninstaller
executable and its directory may remain after removal. See
experiments/win32s91 for the adapters, tests and evidence. Normal TCG in some
QEMU builds faults inside WIN32S16.DLL; use -accel tcg,one-insn-per-tb=on.
