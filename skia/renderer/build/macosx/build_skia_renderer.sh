#!/bin/sh
set -e

source ../../../../dependencies/macosx/config_directories.sh

CONFIG=debug
GRAPHICS=gl
OTHER_OPTIONS=

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
    if [[ $var = "text" ]]; then
        OTHER_OPTIONS+=--with_rive_text
    fi
done

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

if [[ ! -f "$DEPENDENCIES/skia/out/$GRAPHICS/$CONFIG/libskia.a" ]]; then
    pushd $DEPENDENCIES_SCRIPTS
    ./make_viewer_skia.sh $GRAPHICS $CONFIG
    popd
fi

export PREMAKE=$DEPENDENCIES/bin/premake5
pushd ..

$PREMAKE --file=./premake5.lua gmake2 $OTHER_OPTIONS

for var in "$@"; do
    if [[ $var = "clean" ]]; then
        make clean
        make config=release clean
    fi
done

make config=$CONFIG -j$(($(sysctl -n hw.physicalcpu) + 1))

popd
