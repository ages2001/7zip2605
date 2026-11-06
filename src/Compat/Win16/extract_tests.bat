@echo off
C:\7Z16C.EXE l C:\EXTRACT.7Z > C:\X29.TXT
if errorlevel 1 goto fail
C:\7Z16C.EXE x C:\EXTRACT.7Z C:\LONGDEST1 >> C:\X29.TXT
if errorlevel 3 goto fail
if not errorlevel 2 goto fail
if exist C:\LONGDEST\NUL goto fail
C:\7Z16C.EXE x C:\EXTRACT.7Z C:\X29OUT >> C:\X29.TXT
if errorlevel 1 goto fail
C:\7Z16C.EXE x C:\EXTRACT.7Z C:\X29OUT >> C:\X29.TXT
if errorlevel 3 goto fail
if not errorlevel 2 goto fail
C:\7Z16C.EXE x C:\MULTI.7Z C:\X29BAD >> C:\X29.TXT
if errorlevel 3 goto fail
if not errorlevel 2 goto fail
C:\7Z16C.EXE x C:\BADCRC.7Z C:\X29CRC >> C:\X29.TXT
if errorlevel 3 goto fail
if not errorlevel 2 goto fail
C:\7Z16C.EXE t C:\EXTRACT.7Z >> C:\X29.TXT
if errorlevel 1 goto fail
echo PASS ALL CLI EXTRACTION TESTS >> C:\X29.TXT
goto end
:fail
echo FAIL CLI EXTRACTION TESTS >> C:\X29.TXT
:end
