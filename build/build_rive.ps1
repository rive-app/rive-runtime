# requires Set-ExecutionPolicy Bypass -Scope CurrentUser
$CWD = Get-Location
if (Get-Command -Name "msbuild" -ErrorAction SilentlyContinue){}
else
{
    $Rive_Runtime_Root = Split-Path -Parent $PSScriptRoot
    & $Rive_Runtime_Root\..\..\scripts\setup_windows_dev.ps1
}
Set-Location $CWD
sh build_rive.sh $args