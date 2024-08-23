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

mkdir -p glfw_build
cd glfw_build
# On Windows, match Skia's /MT flag for link compatibility.
cmake ../glfw -DBUILD_SHARED_LIBS=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DGLFW_BUILD_WAYLAND=OFF
cmake --build . --parallel --config Release
