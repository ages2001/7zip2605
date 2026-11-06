@echo off
C:\7Z16C l C:\MULTI.7Z > C:\C16LIST.TXT
if errorlevel 1 goto fail
C:\7Z16C t C:\MULTI.7Z > C:\C16MULTI.TXT
if errorlevel 1 goto fail
C:\7Z16C t C:\COPY.7Z > C:\C16COPY.TXT
if errorlevel 1 goto fail
C:\7Z16C t C:\BADCRC.7Z > C:\C16CRC.TXT
if not errorlevel 2 goto fail
if errorlevel 3 goto fail
C:\7Z16C t C:\TRUNC.7Z > C:\C16TRUNC.TXT
if not errorlevel 2 goto fail
if errorlevel 3 goto fail
C:\7Z16C x C:\MULTI.7Z > C:\C16USAGE.TXT
if not errorlevel 7 goto fail
if errorlevel 8 goto fail
echo PASS ALL CONSOLE TESTS > C:\C16PASS.TXT
goto done
:fail
echo FAIL CONSOLE TESTS > C:\C16PASS.TXT
:done
