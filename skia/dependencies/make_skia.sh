#!/bin/sh

set -e

# Requires depot_tools and git: 
#   https://skia.org/user/download
# Build notes:
#   https://skia.org/user/build
# GLFW requires CMake

SKIA_REPO=https://github.com/google/skia
SKIA_STABLE_BRANCH=chrome/m87

# -----------------------------
# Get & Build Skia
# -----------------------------
if [ ! -d skia ]; then
	echo "Cloning Skia."
    git clone $SKIA_REPO
else
    echo "Already have Skia, update it."
    cd skia && git checkout master && git fetch && git pull
    cd ..
fi

cd skia

# switch to a stable branch
echo "Checking out stable branch $SKIA_STABLE_BRANCH"
git checkout $SKIA_STABLE_BRANCH

python tools/git-sync-deps
bin/gn gen out/Static --args=" \
    is_official_build=true \
    skia_use_angle=false \
    skia_use_dng_sdk=false \
    skia_use_egl=false \
    skia_use_expat=false \
    skia_use_fontconfig=false \
    skia_use_freetype=true \
    skia_use_system_freetype2=false \
    skia_use_icu=false \
    skia_use_libheif=false \
    skia_use_system_libpng=false \
    skia_use_libjpeg_turbo_encode=false \
    skia_use_libjpeg_turbo_decode=false \
    skia_use_libwebp_encode=false \
    skia_use_libwebp_decode=false \
    skia_use_lua=false \
    skia_use_piex=false \
    skia_use_vulkan=false \
    skia_use_metal=false \
    skia_use_gl=true \
    skia_use_zlib=true \
    skia_use_system_zlib=false \
    skia_enable_ccpr=true \
    skia_enable_gpu=true \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_use_libpng_encode = true \
    skia_use_libpng_decode = true \
    "
ninja -C out/Static
cd ..