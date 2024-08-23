# requires Set-ExecutionPolicy Bypass -Scope CurrentUser
$CWD = Get-Location
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
Set-Location $CWD
sh build_rive.sh $args