# requires Set-ExecutionPolicy Bypass -Scope CurrentUser
$Rive_Root = Resolve-Path -Path "$PSScriptRoot\..\..\..\"
$Build_Path = Resolve-Path -Path "$PSScriptRoot"
$Env:Path += ";$Build_Path"
$Env:RIVE_ROOT=$Rive_Root
# Add windows to path
$CWD = Get-Location
if (Get-Command -Name "fxc" -ErrorAction SilentlyContinue){}
else
{
    if (Test-Path "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\Launch-VsDevShell.ps1" -PathType Leaf) {
        & "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\Launch-VsDevShell.ps1"
    } else {
        if (Test-Path "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1") {
            & 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1'
        }
        else {
            if (Test-Path "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\Launch-VsDevShell.ps1") {
                & 'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\Launch-VsDevShell.ps1'
            }
            else {
                # No hardcoded VS2022 edition found. Fall back to vswhere, which
                # locates any VS install (2022, 2026, BuildTools, custom path).
                # Constrain to VS2022+ (-version "[17.0,)"): VS2022 is v17,
                # VS2026 is v18; this excludes VS2019 (v16) and older, which
                # can't build Rive.
                $vswhere = "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
                $vsDevShell = $null
                if (Test-Path $vswhere) {
                    $vsPath = & $vswhere -latest -prerelease -products * `
                        -version "[17.0,)" `
                        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                        -property installationPath
                    if ($vsPath -and (Test-Path "$vsPath\Common7\Tools\Launch-VsDevShell.ps1")) {
                        $vsDevShell = "$vsPath\Common7\Tools\Launch-VsDevShell.ps1"
                    }
                }
                if ($vsDevShell) {
                    & $vsDevShell
                } else {
                    Write-Error "Visual Studio (2022 or newer, with the 'Desktop development with C++' workload) does not appear to be installed. Install it, or install to C:\Program Files\Microsoft Visual Studio."
                    exit
                }
            }
        }
    }
}
Set-Location $CWD