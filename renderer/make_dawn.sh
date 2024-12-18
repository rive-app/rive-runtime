#!/bin/sh

set -e

mkdir -p dependencies
cd dependencies

if [ ! -d dawn ]; then
	echo "Cloning Dawn..."
    git clone https://dawn.googlesource.com/dawn
else
    echo "Already have Dawn; updating it..."
    git -C dawn fetch origin
fi

cd dawn
git checkout origin/main
cp scripts/standalone.gclient .gclient
gclient sync
gn gen --args='is_debug=false dawn_complete_static_libs=true use_custom_libcxx=true dawn_use_swiftshader=false angle_enable_swiftshader=false' out/release
ninja -C out/release -j20 webgpu_dawn_static cpp proc_static
