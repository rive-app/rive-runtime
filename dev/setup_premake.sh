#!/bin/bash

unameOut="$(uname -s)"
case "${unameOut}" in
Linux*) MACHINE=linux ;;
Darwin*) MACHINE=mac ;;
CYGWIN*) MACHINE=cygwin ;;
MINGW*) MACHINE=mingw ;;
*) MACHINE="UNKNOWN:${unameOut}" ;;
esac

# check if use has already installed premake5
if ! command -v premake5 &>/dev/null; then
    # no premake found in path
    if [[ ! -f "bin/premake5" ]]; then
        mkdir -p bin
        pushd bin
        echo Downloading Premake5
        if [ "$MACHINE" = 'mac' ]; then
            PREMAKE_URL=https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-macosx.tar.gz
        else
            PREMAKE_URL=https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz
        fi
        curl $PREMAKE_URL -L -o premake.tar.gz
        # Export premake5 into bin
        tar -xvf premake.tar.gz 2>/dev/null
        # Delete downloaded archive
        rm premake.tar.gz
        popd
    fi
    export PREMAKE=$PWD/bin/premake5
else
    export PREMAKE=premake5
fi
