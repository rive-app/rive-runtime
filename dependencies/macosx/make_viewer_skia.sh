#!/bin/sh

set -e

if [[ -z "${DEPENDENCIES}" ]]; then
    echo "DEPENDENCIES env variable must be set. This script is usually called by other scripts."
    exit 1
fi

pushd $DEPENDENCIES

if [ ! -d skia ]; then
    git clone https://github.com/google/skia skia
    cd skia
    git checkout chrome/m99
else
    cd skia
fi

python tools/git-sync-deps

CONFIG=debug
RENDERER=
SKIA_USE_GL=false
SKIA_USE_METAL=false

for var in "$@"; do
    if [[ $var = "release" ]]; then
        CONFIG=release
    fi
    if [[ $var = "gl" ]]; then
        SKIA_USE_GL=true
        RENDERER=gl
    fi
    if [[ $var = "metal" ]]; then
        SKIA_USE_METAL=true
        RENDERER=metal
    fi
done

if [[ $CONFIG = "debug" ]]; then
    bin/gn gen out/$RENDERER/debug --type=static_library --args=" \
    extra_cflags=[\"-fno-rtti\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"] \

    is_official_build=false \
    skia_use_gl=$SKIA_USE_GL \
    skia_use_zlib=true \
    skia_enable_gpu=true \
    skia_enable_fontmgr_empty=false \
    skia_use_libpng_encode=true \
    skia_use_libpng_decode=true \
    skia_enable_skgpu_v1=true \

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
    skia_use_libwebp_decode=true \
    skia_use_system_libwebp=false \
    skia_use_lua=false \
    skia_use_piex=false \
    skia_use_vulkan=false \
    skia_use_metal=$SKIA_USE_METAL \
    skia_use_angle=false \
    skia_use_system_zlib=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_enable_skottie=false \
    skia_enable_tools=false \
    "
    ninja -C out/$RENDERER/debug
    du -hs out/$RENDERER/debug/libskia.a
fi

if [[ $CONFIG = "release" ]]; then
    bin/gn gen out/$RENDERER/release --type=static_library --args=" \
    extra_cflags=[\"-fno-rtti\", \"-flto=full\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"] \

    is_official_build=true \
    skia_use_gl=true \
    skia_use_zlib=true \
    skia_enable_gpu=true \
    skia_enable_fontmgr_empty=false \
    skia_use_libpng_encode=true \
    skia_use_libpng_decode=true \
    skia_enable_skgpu_v1=true \

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
    skia_use_libwebp_decode=true \
    skia_use_system_libwebp=false \
    skia_use_lua=false \
    skia_use_piex=false \
    skia_use_vulkan=false \
    skia_use_metal=true \
    skia_use_angle=false \
    skia_use_system_zlib=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_enable_skottie=false \
    skia_enable_tools=false \
    "
    ninja -C out/$RENDERER/release
    du -hs out/$RENDERER/release/libskia.a
fi
