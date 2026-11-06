@echo off
7Z i > INFO.TXT
if errorlevel 1 goto fail
7Z a -md1m TEST.7Z INPUT.TXT > RUN.TXT
if errorlevel 1 goto fail
7Z t TEST.7Z >> RUN.TXT
if errorlevel 1 goto fail
7Z x TEST.7Z -oOUT >> RUN.TXT
if errorlevel 1 goto fail
7Z a -tzip TEST.ZIP INPUT.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z t TEST.ZIP >> RUN.TXT
if errorlevel 1 goto fail
7Z a -txz -md1m TEST.XZ INPUT.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z t TEST.XZ >> RUN.TXT
if errorlevel 1 goto fail
7Z a -ttar TEST.TAR INPUT.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z t TEST.TAR >> RUN.TXT
if errorlevel 1 goto fail
7Z u -md1m TEST.7Z SECOND.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z rn TEST.7Z SECOND.TXT RENAMED.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z d TEST.7Z INPUT.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z x TEST.7Z -oUPDATED >> RUN.TXT
if errorlevel 1 goto fail
7Z h -scrcSHA256 INPUT.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z a -md1m C:\D32\ABS.7Z C:\D32\INPUT.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z t C:ABS.7Z >> RUN.TXT
if errorlevel 1 goto fail
7Z a -md1m -pTestOnly29 -mhe=on SECRET.7Z INPUT.TXT >> RUN.TXT
if errorlevel 1 goto fail
7Z t -pTestOnly29 SECRET.7Z >> RUN.TXT
if errorlevel 1 goto fail
7Z x -pTestOnly29 SECRET.7Z -oSECRET >> RUN.TXT
if errorlevel 1 goto fail
7Z t -pWrong SECRET.7Z >> RUN.TXT
if errorlevel 3 goto fail
if not errorlevel 2 goto fail
7Z x TEST.7Z -oLONGDEST1 >> RUN.TXT
if errorlevel 3 goto fail
if not errorlevel 2 goto fail
if exist LONGDEST\NUL goto fail
echo PASS DOS CLI COMMAND TESTS >> RUN.TXT
goto end
:fail
echo FAIL DOS CLI COMMAND TESTS >> RUN.TXT
:end
echo DONE > DONE.TXT
