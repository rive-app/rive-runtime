#!/bin/sh

set -e

# Requires depot_tools and git:
#   https://skia.org/user/download
# Build notes:
#   https://skia.org/user/build
# GLFW requires CMake
getSkia() {
    SKIA_REPO=$1
    SKIA_STABLE_BRANCH=$2
    FOLDER_NAME=$3
    # -----------------------------
    # Get & Build Skia
    # -----------------------------
    if [ ! -d $FOLDER_NAME ]; then
        echo "Cloning Skia into $FOLDER_NAME."
        git clone $SKIA_REPO $FOLDER_NAME
    else
        echo "Already have Skia in $FOLDER_NAME, update it."
        cd $FOLDER_NAME && git fetch && git pull
        cd ..
    fi

    cd $FOLDER_NAME

    # switch to a stable branch
    echo "Checking out stable branch $SKIA_STABLE_BRANCH"
    git checkout $SKIA_STABLE_BRANCH
    python tools/git-sync-deps
    cd ..
}

if [ $# -eq 0 ]; then
    # No arguments supplied; checkout the default skia repos.
    getSkia https://github.com/rive-app/skia rive skia_rive_optimized
    getSkia https://github.com/google/skia chrome/m99 skia
else
    # The caller specified which skia repo to checkout.
    getSkia $@
fi
