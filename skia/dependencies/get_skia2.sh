#!/bin/bash

set -e

# Requires depot_tools and git: 
#   https://skia.org/user/download
# Build notes:
#   https://skia.org/user/build
# GLFW requires CMake

SKIA_REPO="${SKIA_REPO:-https://github.com/rive-app/skia}"
SKIA_BRANCH="${SKIA_BRANCH:-rive}"

# note: we have imports in recorder that rely on this being "skia"
# either: change that in recorder, or check out skia into a subfolder
# both have consequences. 
SKIA_DIR_NAME="${SKIA_DIR_NAME:-skia}"

_skiaExists() {
    if test -d $SKIA_DIR_NAME/.git; then 
        return 0
    else 
        return 1
    fi
}

_skiaHasExpectedRemote() {
    if test -d $SKIA_DIR_NAME/.git; then
        pushd $SKIA_DIR_NAME
        if git remote -v |grep "$SKIA_REPO"; then
            popd
            return 0
        else 
            popd
            echo "Skia it is using the wrong remote."
            return 1
        fi 
    else
        echo "Skia is expected to be a git repo"
        return 1
    fi 
}

_updateSkia() {
    pushd $SKIA_DIR_NAME
    git fetch && git pull
    popd
}

_cloneSkia() {
    echo "Cloning Skia [$SKIA_REPO : $SKIA_BRANCH] into $SKIA_DIR_NAME."
    git clone $SKIA_REPO $SKIA_DIR_NAME
}

_removeSkia(){
    echo "Removing skia folder."
    rm -rf $SKIA_DIR_NAME
}

getSkia () {
    # -----------------------------
    # Get Skia:
    # -----------------------------
    if _skiaExists; then 
        if _skiaHasExpectedRemote; then 
            _updateSkia
        else 
            _removeSkia
            _cloneSkia
        fi 
    else 
        _removeSkia
        _cloneSkia
    fi 

    pushd $SKIA_DIR_NAME

    echo "Checking out branch $SKIA_BRANCH"
    git checkout $SKIA_BRANCH
    # Remove piet-gpu from dependencies because repository does not exist anymore
    sed -i.bak -r 's/.*piet.*//g' "$PWD/DEPS"


    python tools/git-sync-deps

    popd 
}