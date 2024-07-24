#!/bin/sh

set -e

mkdir -p dependencies
cd dependencies

if [ ! -d dawn ]; then
	echo "Cloning Dawn..."
    git clone https://dawn.googlesource.com/dawn
    cd dawn
    cp scripts/standalone.gclient .gclient
    gclient sync
    gn gen --args='is_debug=false dawn_complete_static_libs=true use_custom_libcxx=false dawn_use_swiftshader=false angle_enable_swiftshader=false' out/release
else
    echo "Already have Dawn; updating it..."
    cd dawn
    git fetch origin
    git checkout origin/main
    gclient sync
fi

ninja -C out/release -j20 webgpu_dawn_static cpp proc_static
