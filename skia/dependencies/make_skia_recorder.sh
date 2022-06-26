#!/bin/bash
set -ex

SKIA_REPO=https://github.com/rive-app/skia
SKIA_BRANCH=rive
SKIA_DIR=${SKIA_DIR:-skia}

# -----------------------------
# Get & Build Skia
# -----------------------------
if test -d $SKIA_DIR/.git; then
    echo "Skia Recorder is a .git repo"
else
    echo "Skia Recorder is not a .git repo, probably a cache, cleaning it out"
    rm -rf $SKIA_DIR
fi 

if [ ! -d $SKIA_DIR ]; then
	echo "Cloning Skia recorder."
    git clone -j8 $SKIA_REPO $SKIA_DIR
    cd $SKIA_DIR
    git checkout $SKIA_BRANCH
else
    echo "Already have Skia recorder, update it."
    cd $SKIA_DIR
    git checkout $SKIA_BRANCH
    git fetch
    git pull
fi

python tools/git-sync-deps

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
      \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"\
    ] \
    rive_use_picture=true \
    skia_use_angle=false \
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
