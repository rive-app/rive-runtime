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
git checkout 50f469b60b89ac3575abc43f1d6bbe7dcd39e647
cp scripts/standalone.gclient .gclient
gclient sync -f -D
gn gen --args='is_debug=false dawn_complete_static_libs=true use_custom_libcxx=false dawn_use_swiftshader=false angle_enable_swiftshader=false' out/release
ninja -C out/release -j20 webgpu_dawn_static cpp proc_static
