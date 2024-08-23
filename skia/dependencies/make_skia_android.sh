#!/bin/bash

set -ex

export MAKE_SKIA_FILE="$0"
source ./get_skia2.sh
source ./cache_helper.sh

ARCH=$1
CONFIG=$2

build_skia_android() {
    cd "$SKIA_DIR_NAME"

    if [ "$ARCH" != "x86" ] &&
        [ "$ARCH" != "x64" ] &&
        [ "$ARCH" != "arm" ] &&
        [ "$ARCH" != "arm64" ]; then
        printf "Invalid architecture: '%s'. Choose one between 'x86', \
            'x64', \
            'arm', \
            or 'arm64'" "$ARCH"
        exit 1
    fi

    BUILD_FLAGS=
    EXTRA_CFLAGS=
    if [ "$CONFIG" = "debug" ]; then
        BUILD_FLAGS="is_official_build=false is_debug=true skia_enable_tools=false"
    else # release
        BUILD_FLAGS="is_official_build=true is_debug=false"
        EXTRA_CFLAGS="\
    \"-fno-rtti\",                          \
    \"-flto=full\",                         \
    \"-fembed-bitcode\",                    \
    \"-DRIVE_OPTIMIZED\",                   \
    \"-DSK_DISABLE_SKPICTURE\",             \
    \"-DSK_DISABLE_TEXT\",                  \
    \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\",  \
    \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\",  \
    \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \
    \"-DSK_DISABLE_AAA\",                   \
    \"-DSK_DISABLE_EFFECT_DESERIALIZATION\" \
    "
    fi

    # Useful for debugging:
    # bin/gn args --list out/${ARCH}

    bin/gn gen out/"${CONFIG}"/"${ARCH}" --args=" \
        ndk=\"${NDK_PATH}\" \
        target_cpu=\"${ARCH}\" \
        extra_cflags=[                              \
            ${EXTRA_CFLAGS}                         \
            ]                                       \
        \
        ${BUILD_FLAGS} \

        skia_gl_standard=\"gles\" 
        skia_use_zlib=true \
        skia_use_egl=true \
        skia_use_gl=true \
        skia_enable_gpu=true \
        skia_use_libpng_decode=false \
        skia_use_libpng_encode=false \

        skia_use_angle=false \
        skia_use_dng_sdk=false \
        skia_use_freetype=false \
        
        skia_use_expat=false \
        skia_use_fontconfig=false \
        skia_use_system_freetype2=false \
        skia_use_icu=false \
        skia_use_libheif=false \
        skia_use_system_libpng=false \
        skia_use_system_libjpeg_turbo=false \
        skia_use_system_libwebp=false \
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
        
        skia_enable_skshaper=false \
        "

    ninja -C out/"${CONFIG}"/"${ARCH}"
    cd ..
}

if is_build_cached_locally; then
    echo "Build is cached, nothing to do."
else
    if is_build_cached_remotely; then
        pull_cache
    else
        getSkia
        build_skia_android
        # (umberto) commenting this out for now for CMake builds.
        # hmm not the appiest with this guy
        # if [ "$CONFIG" != "debug" ]; then
        OUTPUT_CACHE=out/"${CONFIG}"/$ARCH upload_cache
        # fi
    fi
fi
