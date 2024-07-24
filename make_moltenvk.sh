#!/bin/sh

set -e

mkdir -p dependencies
cd dependencies

if [ ! -d MoltenVK ]; then
    echo "Cloning MoltenVK..."
    git clone https://github.com/rive-app/MoltenVK.git
else
    echo "Already have MoltenVK..."
fi

cd MoltenVK

echo "Fetching dependencies..."
./fetchDependencies --macos

echo "Building branch with experimental support for VK_EXT_rasterization_order_attachment_access..."
git checkout origin/VK_EXT_rasterization_order_attachment_access
xcodebuild -project MoltenVKPackaging.xcodeproj -scheme "MoltenVK Package (macOS only)" -configuration "Release"
