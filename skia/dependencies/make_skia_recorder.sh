#!/bin/bash
set -ex

export MAKE_SKIA_FILE="$0"
source ./get_skia2.sh
source ./cache_helper.sh

build_skia_recorder() {
    cd $SKIA_DIR_NAME

    # This header is required so Skia can continue to use its internal copy of libpng while we link
    # with our own.
    cp ../../pngprefix/pngprefix.h third_party/externals/libpng/
    echo '#include <pngprefix.h>' >> third_party/libpng/pnglibconf.h
    
    bin/gn gen out/static --args=" \
        is_official_build=true \
        extra_cflags=[
        \"-fno-rtti\",\
        \"-DSK_DISABLE_SKPICTURE\",\
        \"-DSK_DISABLE_TEXT\",\
        \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\",\
        \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\",\
        \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\",\
        \"-DSK_DISABLE_AAA\",\
        \"-DSK_DISABLE_EFFECT_DESERIALIZATION\",\
        \"-DPNG_PREFIX=sk_\"\
        ]\
        rive_use_picture=true \
        skia_use_angle=false \
        skia_use_dng_sdk=false \
        skia_use_egl=false \
        skia_use_expat=true \
        skia_use_system_expat=false \
        skia_enable_svg=true \
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
        skia_gl_standard = \"\" \
        "
    ninja -C out/static

    cd ..
}

if is_build_cached_locally; then 
    echo "Build is cached, nothing to do."
else
    if is_build_cached_remotely; then 
        pull_cache
    else 
        getSkia
        build_skia_recorder
        OUTPUT_CACHE=out upload_cache
    fi 
fi
