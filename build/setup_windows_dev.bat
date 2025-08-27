@echo off
SET SCRIPT_PATH=%~dp0
SET PATH=%PATH%;%SCRIPT_PATH%
echo %SCRIPT_PATH%
pushd %SCRIPT_PATH%\..\..\..\
SET RIVE_ROOT=%CD%
echo %CD%
popd
WHERE fxc >nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
    REM we could check where vs is install via the registery like stated here https://superuser.com/questions/539666/how-to-get-the-visual-studio-installation-path-in-a-batch-file but i think that is overkill
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" (
        CALL "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    ) else (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" ( 
            CALL "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
        ) else echo "Visual Studio 2022 does not appear to be installed, please install visual studio to C:\Program Files\Microsoft Visual Studio"
    )
)