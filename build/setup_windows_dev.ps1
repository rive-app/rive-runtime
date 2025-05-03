# requires Set-ExecutionPolicy Bypass -Scope CurrentUser
$Rive_Root = Resolve-Path -Path "$PSScriptRoot\..\..\..\"
$Build_Path = "$Rive_Root"+"\packages\runtime\build"
$Env:Path += ";" + $Build_Path
$Env:RIVE_ROOT=$Rive_Root
# Add windows to path
$CWD = Get-Location
if (Get-Command -Name "fxc" -ErrorAction SilentlyContinue){}
else
{
    if (Test-Path "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\Launch-VsDevShell.ps1" -PathType Leaf) {
        & "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\Launch-VsDevShell.ps1"
    } else {
        if ("C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1") {
            & 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1'
        }
        else {
            Write-Error "Visual Studio 2022 does not appear to be installed, please install visual studio to C:\Program Files\Microsoft Visual Studio"
            exit
        }
    }
}
Set-Location $CWD