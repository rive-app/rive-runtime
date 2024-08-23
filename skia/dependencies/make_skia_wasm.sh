#!/bin/sh

set -e

# Requires depot_tools and git:
#   https://skia.org/user/download
# Build notes:
#   https://skia.org/user/build
# GLFW requires CMake

if [ -z "${PAT_GITHUB}" ]; then
    SKIA_URL=git@github.com:rive-app/skia.git
else
    SKIA_URL=https://${PAT_GITHUB}@github.com/rive-app/skia.git
fi
./get_skia.sh ${SKIA_URL} rive skia_rive_optimized

cd skia_rive_optimized

# build static for host
bin/gn gen out/wasm --type=static_library --args=" \
    cc=\"emcc\" \
    cxx=\"em++\" \
    ar=\"emar\" \
    is_official_build=true \
    target_cpu=\"wasm\" \
    skia_use_angle=false \
    skia_use_webgl=true \
    extra_cflags=[\"-s\", \"-fno-rtti\", \"-flto=full\", \"-DSK_RELEASE\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\", \"-DSK_SUPPORT_GPU=1\", \"-DSK_GL\", \"-DSK_DISABLE_TRACING\", \"-DSK_NO_FONTS\", \"-DSK_FORCE_8_BYTE_ALIGNMENT\"] \
    skia_enable_fontmgr_custom_embedded=false \
    skia_enable_fontmgr_custom_empty=false \
    skia_use_dng_sdk=false \
    skia_use_egl=false \
    skia_use_expat=false \
    skia_use_fontconfig=false \
    skia_use_freetype=false \
    skia_use_icu=false \
    skia_use_libheif=false \
    skia_use_system_libpng=false \
    skia_use_system_libjpeg_turbo=false \
    skia_use_libjpeg_turbo_encode=false \
    skia_use_libjpeg_turbo_decode=true \
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
    skia_gl_standard=\"webgl\" \
    skia_enable_fontmgr_empty=true \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_use_libpng_encode = true \
    skia_use_libpng_decode = true \
    skia_enable_skottie = false \
    skia_enable_tools = false \
    skia_enable_skgpu_v1 = true \
    "

ninja -C out/wasm libskia.a
du -hs out/wasm/libskia.a

cd ..
