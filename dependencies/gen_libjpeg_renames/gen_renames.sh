#!/bin/bash
set -e

# This script should be called on a Mac!

# NOTE: Before building, jconfig.txt needs to be renamed to jconfig.h

if [[ ! -f "dependencies/bin/premake5" ]]; then
    mkdir -p dependencies/bin
    pushd dependencies
    # v5.0.0-beta2 doesn't support apple silicon properly, update the branch
    # once a stable one is avaialble that supports it
    git clone --depth 1 --branch master https://github.com/premake/premake-core.git
    pushd premake-core
    if [[ $LOCAL_ARCH == "arm64" ]]; then
        PREMAKE_MAKE_ARCH=ARM
    else
        PREMAKE_MAKE_ARCH=x86
    fi
    make -f Bootstrap.mak osx PLATFORM=$PREMAKE_MAKE_ARCH
    cp bin/release/* ../bin
    popd
    popd
fi

export PREMAKE=$PWD/dependencies/bin/premake5

for var in "$@"; do
    if [[ $var = "clean" ]]; then
        echo 'Cleaning...'
        rm -fR out
    fi
done

mkdir -p out
mkdir -p out_with_renames

pushd ../../../
PACKAGES=$PWD
popd
export PREMAKE_PATH="dependencies/export-compile-commands":"$PACKAGES/runtime_unity/native_plugin/platform":"$PACKAGES/runtime/build":"$PREMAKE_PATH"

$PREMAKE gmake2 --file=../premake5_libjpeg_v2.lua --out=out --no-libjpeg-renames
pushd out
make -j$(($(sysctl -n hw.physicalcpu) + 1))
popd

nm out/liblibjpeg.a --demangle &>libjpeg_names.txt

dart gen_header.dart

# make with renames just to examine the exported symbols...
$PREMAKE gmake2 --file=../premake5_libjpeg_v2.lua --out=out_with_renames
pushd out_with_renames
make -j$(($(sysctl -n hw.physicalcpu) + 1))
popd

nm out_with_renames/liblibjpeg.a --demangle &>libjpeg_renames.txt
