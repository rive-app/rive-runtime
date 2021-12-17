#!/bin/sh
set -e

./get_skia.sh

cd skia


ARCH=$1

if [ "$ARCH" != "x86" ] &&
    [ "$ARCH" != "x64" ] &&
    [ "$ARCH" != "arm" ] &&
    [ "$ARCH" != "arm64" ]; then
    printf "Invalid architecture: '%s'. Choose one between 'x86', 'x64', 'arm', or 'arm64'" "$ARCH"
    exit 1
fi

# Useful for debugging:
# bin/gn args --list out/${ARCH}

bin/gn gen out/"${ARCH}" --args=" \
    ndk=\"${NDK_PATH}\" \
    target_cpu=\"${ARCH}\" \
    extra_cflags=[\"-fno-rtti\", \"-fembed-bitcode\", \"-flto=full\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"] \

    skia_gl_standard=\"gles\" 
    is_official_build=true \
    skia_use_zlib=true \
    skia_use_egl=true \
    skia_use_gl=true \
    skia_enable_gpu=true \
    skia_use_libpng_decode=true \
    skia_use_libpng_encode=true \


    skia_use_angle=false \
    skia_use_dng_sdk=false \
    
    skia_use_expat=false \
    skia_use_fontconfig=false \
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
    
    skia_use_system_zlib=false \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_enable_skottie=false \
    "

ninja -C out/"${ARCH}"
cd ..