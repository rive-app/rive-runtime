#!/bin/bash
set -e

BASEDIR="$PWD"

if [ -d "$PWD/../../../rive-cpp" ]; then
    export RIVE_RUNTIME_DIR="$PWD/../../../rive-cpp"
else
    export RIVE_RUNTIME_DIR="$PWD/../../../runtime"
fi

cd ../renderer
./build.sh "$@"

cd "$BASEDIR"

cd build

OPTION=$1

if [ "$OPTION" = 'help' ]; then
    echo build.sh - build debug library
    echo build.sh clean - clean the build
    echo build.sh release - build release library
elif [ "$OPTION" = "clean" ]; then
    echo Cleaning project ...
    premake5 clean --scripts="$RIVE_RUNTIME_DIR/build"
elif [ "$OPTION" = "release" ]; then
    premake5 gmake --scripts="$RIVE_RUNTIME_DIR/build" --with_rive_text && make config=release -j7
else
    premake5 gmake --scripts="$RIVE_RUNTIME_DIR/build" --with_rive_text && make -j7
fi
