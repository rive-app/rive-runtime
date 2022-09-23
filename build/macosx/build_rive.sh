#!/bin/sh
set -e

source ../../dependencies/macosx/config_directories.sh

CONFIG=debug
TOOLS=
for var in "$@"; do
    if [[ $var = "release" ]]; then
        CONFIG=release
    fi
    if [[ $var = "tools" ]]; then
        TOOLS="--with_rive_tools"
    fi
    if [[ $var = "text" ]]; then
        TOOLS="--with_rive_text"
    fi
done

if [[ ! -f "$DEPENDENCIES/bin/premake5" ]]; then
    pushd $DEPENDENCIES_SCRIPTS
    ./get_premake5.sh
    popd
fi

export PREMAKE=$DEPENDENCIES/bin/premake5
pushd ..

$PREMAKE --file=./premake5.lua gmake2 $TOOLS

for var in "$@"; do
    if [[ $var = "clean" ]]; then
        make clean
        make config=release clean
    fi
done

make config=$CONFIG -j$(($(sysctl -n hw.physicalcpu) + 1))

popd
