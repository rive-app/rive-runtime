#!/bin/sh

set -e

mkdir -p dependencies
cd dependencies

if [ ! -d swiftshader ]; then
    echo "Cloning swiftshader..."
    git clone https://github.com/google/swiftshader.git
else
    echo "Already have swiftshader; updating it..."
    git -C swiftshader fetch origin
    git -C swiftshader checkout origin/main
fi

cd swiftshader/build
cmake ..
cmake --build . --parallel
