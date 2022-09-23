@echo off
pushd %DEPENDENCIES%
if not exist ".\bin" mkdir bin
echo Downloading Premake5
curl https://github.com/premake/premake-core/releases/download/v5.0.0-beta1/premake-5.0.0-beta1-windows.zip -L -o .\bin\premake_windows.zip 
pushd bin
:: Export premake5 into bin
tar -xf premake_windows.zip
:: Delete downloaded archive
del premake_windows.zip
popd
popd