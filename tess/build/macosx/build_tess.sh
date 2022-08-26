#!/bin/sh
set -e

source ../../../dependencies/macosx/config_directories.sh

if [[ ! -f "$DEPENDENCIES/bin/premake5" ]]; then
    pushd $DEPENDENCIES_SCRIPTS
    ./get_premake5.sh
    popd
fi

if [[ ! -d "$DEPENDENCIES/sokol" ]]; then
    pushd $DEPENDENCIES_SCRIPTS
    ./get_sokol.sh
    popd
fi

if [[ ! -d "$DEPENDENCIES/earcut.hpp" ]]; then
    pushd $DEPENDENCIES_SCRIPTS
    ./get_earcut.sh
    popd
fi

if [[ ! -d "$DEPENDENCIES/libtess2" ]]; then
    pushd $DEPENDENCIES_SCRIPTS
    ./get_libtess2.sh
    popd
fi

export PREMAKE=$DEPENDENCIES/bin/premake5
pushd ..

CONFIG=debug
GRAPHICS=gl
TEST=false
for var in "$@"; do
    if [[ $var = "release" ]]; then
        CONFIG=release
    fi
    if [[ $var = "gl" ]]; then
        GRAPHICS=gl
    fi
    if [[ $var = "d3d" ]]; then
        GRAPHICS=d3d
    fi
    if [[ $var = "metal" ]]; then
        GRAPHICS=metal
    fi
    if [[ $var = "test" ]]; then
        TEST=true
    fi
done

$PREMAKE --file=./premake5_tess.lua gmake2 --graphics=$GRAPHICS --with_rive_tools

for var in "$@"; do
    if [[ $var = "clean" ]]; then
        make clean
        make config=release clean
    fi
done

# compile shaders
$DEPENDENCIES/bin/sokol-shdc --input ../src/sokol/shader.glsl --output ../src/sokol/generated/shader.h --slang glsl330:hlsl5:metal_macos

make config=$CONFIG -j$(($(sysctl -n hw.physicalcpu) + 1))

if [[ $TEST = "true" ]]; then
    macosx/bin/$CONFIG/rive_tess_tests
fi
popd
