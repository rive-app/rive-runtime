#!/bin/sh

set -e

# Requires depot_tools and git: 
#   https://skia.org/user/download
# Build notes:
#   https://skia.org/user/build
# GLFW requires CMake

SKIA_REPO=https://github.com/rive-app/skia
SKIA_STABLE_BRANCH=rive

# -----------------------------
# Get & Build Skia
# -----------------------------
if [ ! -d skia ]; then
	echo "Cloning Skia."
    git clone $SKIA_REPO skia
else
    echo "Already have Skia, update it."
    cd skia && git fetch && git pull
    cd ..
fi

cd skia

# switch to a stable branch
echo "Checking out stable branch $SKIA_STABLE_BRANCH"
git checkout $SKIA_STABLE_BRANCH

python tools/git-sync-deps
