#!/bin/bash
set -e

BASEDIR="$PWD"

if [ -d "$PWD/../../../rive-runtime" ]; then
    export RIVE_RUNTIME_DIR="$PWD/../../../rive-runtime"
else
    export RIVE_RUNTIME_DIR="$PWD/../../../runtime"
fi

source "$RIVE_RUNTIME_DIR"/dependencies/config_directories.sh

export SKIA_REPO="https://github.com/rive-app/skia.git"
export SKIA_BRANCH="bae2881014cb5c3216184cbb0b639045b8804931"
export SKIA_COMMIT=$SKIA_BRANCH
# could call this thumbnailer, but we can share.
export CACHE_NAME="rive_recorder"
export MAKE_SKIA_FILE="make_skia_recorder.sh"
export SKIA_DIR_NAME="skia"
export SKIA_DIR="skia"

build_deps() {
    pushd "$RIVE_RUNTIME_DIR"/skia/dependencies
    ./make_skia_recorder.sh
    popd
    echo "$RIVE_RUNTIME_DIR"
    pwd
}

build_deps
cd build

if [[ $1 != "skip" ]]; then
    build_rive.sh $@ 
fi
