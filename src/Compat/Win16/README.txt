
Revision83 adds Deflate64 decoding (ZIP9 / 7z040109), complete 64KiB history
and extended lengths. All five Windows modes and DOS5/6/7 pass bounded native
tests, independent bytes, corruption/cancellation and 253 cache fault checks;
host 171 Deflate64 cases plus ordinary Deflate regressions pass. See
Compat/Win16/test-results/deflate64-83. Encoding and broader parity remain
unfinished; installer82 still embeds GUI/CLI81 pending a later refresh.

Revision84 adds ZIP Deflate64 creation/update: GUI method selection and CLI
-m0=deflate64, with the existing 8 KiB encoder dictionary. A real-mode update
allocation failure was fixed by sharing sequential buffers and releasing
metadata after its last use. All five Windows modes and DOS 5/6/7 now pass
focused native tests and independent content comparisons; encoder and ZIP
transaction fault/regression checks pass. See test-results/deflate64-84 under
Compat/Win16. Full compression settings, broader parity and final packaging
remain unfinished. Installer82 still embeds GUI/CLI81.
