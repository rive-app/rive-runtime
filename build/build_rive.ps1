# requires Set-ExecutionPolicy Bypass -Scope CurrentUser
$CWD = Get-Location
# use fxc because sometimes msbuild is there but fxc is not. The opposite appears to never be true
if (Get-Command -Name "fxc" -ErrorAction SilentlyContinue){}
else
{
    & $PSScriptRoot\setup_windows_dev.ps1
}

Set-Location $CWD
sh build_rive.sh $args