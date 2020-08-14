#!/bin/sh

set -e

# GLFW requires CMake

GLFW_REPO=https://github.com/glfw/glfw


# -----------------------------
# Get & Build GLFW
# -----------------------------
if [ ! -d glfw ]; then
	echo "Cloning GLFW."
    git clone $GLFW_REPO
else
    echo "Already have GLFW, update it."
    cd glfw && git fetch && git merge master
    cd ..
fi

mkdir glfw_build
cd glfw_build
cmake ../glfw -DBUILD_SHARED_LIBS=OFF
make