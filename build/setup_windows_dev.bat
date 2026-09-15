@echo off
SET SCRIPT_PATH=%~dp0
SET PATH=%PATH%;%SCRIPT_PATH%
echo %SCRIPT_PATH%
pushd %SCRIPT_PATH%\..\..\..\
SET RIVE_ROOT=%CD%
echo %CD%
popd

WHERE fxc >nul 2>&1
IF %ERRORLEVEL% EQU 0 GOTO :eof

REM vswhere ships at a fixed location alongside every Visual Studio since 2017,
REM so ask it where Visual Studio is rather than hardcoding a year and edition.
REM The old hardcoded "2022\Enterprise" and "2022\Community" paths stopped
REM matching when Visual Studio moved to version 18.
SET "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
SET "VSPATH="
IF EXIST "%VSWHERE%" FOR /F "usebackq tokens=*" %%i IN (`"%VSWHERE%" -latest -property installationPath`) DO SET "VSPATH=%%i"

IF DEFINED VSPATH IF EXIST "%VSPATH%\Common7\Tools\VsDevCmd.bat" (
    CALL "%VSPATH%\Common7\Tools\VsDevCmd.bat"
    GOTO :eof
)

echo "Could not locate Visual Studio via vswhere. Install Visual Studio with the"
echo "Desktop development with C++ workload, or run from a developer prompt so"
echo "that fxc is already on PATH."
