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
    cd skia && git checkout master && git fetch && git pull
    cd ..
fi

cd skia

# switch to a stable branch
echo "Checking out stable branch $SKIA_STABLE_BRANCH"
git checkout $SKIA_STABLE_BRANCH

python tools/git-sync-deps
bin/gn gen out/ios64 --type=static_library --args=" \
    target_os=\"ios\" \
    target_cpu=\"arm64\" \
    extra_cflags=[\"-fembed-bitcode\", \"-mios-version-min=10.0\"] \
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
    skia_use_metal=true \
    skia_use_gl=false \
    skia_use_zlib=true \
    skia_use_system_zlib=false \
    skia_enable_gpu=true \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_use_libpng_encode = true \
    skia_use_libpng_decode = true \
    skia_enable_skottie = false \
    skia_skip_codesign = true \
    "
ninja -C out/ios64

bin/gn gen out/ios32 --type=static_library --args=" \
    target_os=\"ios\" \
    target_cpu=\"arm\" \
    extra_cflags=[\"-fembed-bitcode\", \"-mios-version-min=10.0\"] \
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
    skia_use_metal=true \
    skia_use_gl=false \
    skia_use_zlib=true \
    skia_use_system_zlib=false \
    skia_enable_gpu=true \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_use_libpng_encode = true \
    skia_use_libpng_decode = true \
    skia_enable_skottie = false \
    skia_skip_codesign = true \
    "
ninja -C out/ios32

bin/gn gen out/iossim --type=static_library --args=" \
    target_os=\"ios\" \
    target_cpu=\"x64\" \
    extra_cflags=[\"-fembed-bitcode\", \"-mios-version-min=10.0\"] \
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
    skia_use_metal=true \
    skia_use_gl=false \
    skia_use_zlib=true \
    skia_use_system_zlib=false \
    skia_enable_gpu=true \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_use_libpng_encode = true \
    skia_use_libpng_decode = true \
    skia_enable_skottie = false \
    skia_skip_codesign = true \
    "
ninja -C out/iossim

# make fat library, note that the ios64 library is already fat with arm64 and arm64e so we don't specify arch there.
xcrun -sdk iphoneos lipo -create -arch x86_64 out/iossim/libskia.a -arch armv7 out/ios32/libskia.a out/ios64/libskia.a -output out/libskia_ios.a


# build static for host
bin/gn gen out/static --type=static_library --args=" \
    is_official_build=true \
    skia_use_angle=false \
    extra_cflags=[\"-flto=full\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"] \
    skia_use_dng_sdk=false \
    skia_use_egl=false \
    skia_use_expat=false \
    skia_use_fontconfig=false \
    skia_use_freetype=false \
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
    skia_enable_gpu=true \
    skia_enable_fontmgr_empty=true \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_use_libpng_encode = true \
    skia_use_libpng_decode = true \
    skia_enable_skottie = false \
    skia_enable_tools = false \
    skia_enable_skgpu_v1 = true \
    skia_enable_skgpu_v2 = false \
    "
ninja -C out/static
du -hs out/static/libskia.a

cd ..