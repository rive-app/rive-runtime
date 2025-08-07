@echo off
REM use fxc because sometimes msbuild is there but fxc is not. The opposite appears to never be true
WHERE fxc >nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
    call %~p0\..\..\build\setup_windows_dev.bat
)
cd %CD%

sh ./test.sh %*