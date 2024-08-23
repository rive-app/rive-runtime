#!/bin/sh

set -e

export MAKE_SKIA_FILE="$0"
./get_skia.sh

# use Rive optimized/stripped Skia for iOS static libs.
cd skia_rive_optimized

python tools/git-sync-deps
bin/gn gen out/ios64 --type=static_library --args=" \
    target_os=\"ios\" \
    target_cpu=\"arm64\" \
    extra_cflags=[\"-fno-rtti\", \"-fembed-bitcode\", \"-mios-version-min=10.0\", \"-flto=full\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"] \

    is_official_build=true \
    skia_use_freetype=true \
    skia_use_metal=true \
    skia_use_zlib=true \
    skia_enable_gpu=true \
    skia_use_libpng_encode=true \
    skia_use_libpng_decode=true \
    
    skia_use_angle=false \
    skia_use_dng_sdk=false \
    skia_use_egl=false \
    skia_use_expat=false \
    skia_use_fontconfig=false \
    skia_use_system_freetype2=false \
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
    skia_use_gl=false \
    skia_use_system_zlib=false \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_enable_skottie=false \
    "
ninja -C out/ios64

bin/gn gen out/ios32 --type=static_library --args=" \
    target_os=\"ios\" \
    target_cpu=\"arm\" \
    extra_cflags=[\"-fno-rtti\", \"-fembed-bitcode\", \"-mios-version-min=10.0\", \"-flto=full\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"] \

    is_official_build=true \
    skia_use_freetype=true \
    skia_use_metal=true \
    skia_use_zlib=true \
    skia_enable_gpu=true \
    skia_use_libpng_encode=true \
    skia_use_libpng_decode=true \
    skia_skip_codesign=true \
    
    skia_use_angle=false \
    skia_use_dng_sdk=false \
    skia_use_egl=false \
    skia_use_expat=false \
    skia_use_fontconfig=false \
    skia_use_system_freetype2=false \
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
    skia_use_gl=false \
    skia_use_system_zlib=false \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_enable_skottie=false \
    "
ninja -C out/ios32

bin/gn gen out/iossim_x86 --type=static_library --args=" \
    target_os=\"ios\" \
    target_cpu=\"x86\" \
    extra_cflags=[\"-fno-rtti\", \"-fembed-bitcode\", \"-mios-version-min=10.0\", \"-flto=full\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"] \

    is_official_build=true \
    skia_use_freetype=true \
    skia_use_metal=true \
    skia_use_zlib=true \
    skia_enable_gpu=true \
    skia_use_libpng_encode=true \
    skia_use_libpng_decode=true \
    skia_skip_codesign=true \

    skia_use_angle=false \
    skia_use_dng_sdk=false \
    skia_use_egl=false \
    skia_use_expat=false \
    skia_use_fontconfig=false \
    skia_use_system_freetype2=false \
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
    skia_use_gl=false \
    skia_use_system_zlib=false \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_enable_skottie=false \
    "
ninja -C out/iossim_x86

bin/gn gen out/iossim_x64 --type=static_library --args=" \
    target_os=\"ios\" \
    target_cpu=\"x64\" \
    extra_cflags=[\"-fno-rtti\", \"-fembed-bitcode\", \"-mios-version-min=10.0\", \"-flto=full\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\"] \

    is_official_build=true \
    skia_use_freetype=true \
    skia_use_metal=true \
    skia_use_zlib=true \
    skia_enable_gpu=true \
    skia_use_libpng_encode=true \
    skia_use_libpng_decode=true \
    skia_skip_codesign=true \

    skia_use_angle=false \
    skia_use_dng_sdk=false \
    skia_use_egl=false \
    skia_use_expat=false \
    skia_use_fontconfig=false \
    skia_use_system_freetype2=false \
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
    skia_use_gl=false \
    skia_use_system_zlib=false \
    skia_enable_fontmgr_empty=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_enable_skottie=false \
    "
ninja -C out/iossim_x64

bin/gn gen out/iossim_arm64 --type=static_library --args=" \
    target_os=\"ios\" \
    target_cpu=\"arm64\" \
    extra_cflags=[\"-fno-rtti\", \"-fembed-bitcode\", \"-flto=full\", \"-DSK_DISABLE_SKPICTURE\", \"-DSK_DISABLE_TEXT\", \"-DRIVE_OPTIMIZED\", \"-DSK_DISABLE_LEGACY_SHADERCONTEXT\", \"-DSK_DISABLE_LOWP_RASTER_PIPELINE\", \"-DSK_FORCE_RASTER_PIPELINE_BLITTER\", \"-DSK_DISABLE_AAA\", \"-DSK_DISABLE_EFFECT_DESERIALIZATION\", \"--target=arm64-apple-ios13.0.0-simulator\"] \
    extra_ldflags=[\"--target=arm64-apple-ios13.0.0-simulator\"] \
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
ninja -C out/iossim_arm64

# make fat library, note that the ios64 library is already fat with arm64 and arm64e so we don't specify arch there.
xcrun -sdk iphoneos lipo -create -arch armv7 out/ios32/libskia.a out/ios64/libskia.a -output out/libskia_ios.a
xcrun -sdk iphoneos lipo -create -arch x86_64 out/iossim_x64/libskia.a -arch i386 out/iossim_x86/libskia.a out/iossim_arm64/libskia.a -output out/libskia_ios_sim.a

# build regular/full skia
cd ../skia

# build static for host
bin/gn gen out/static --type=static_library --args=" \
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
    skia_use_metal=false \
    skia_use_angle=false \
    skia_use_system_zlib=false \
    skia_enable_spirv_validation=false \
    skia_enable_pdf=false \
    skia_enable_skottie=false \
    skia_enable_tools=false \
    skia_enable_skgpu_v2=false \
    skia_gl_standard = \"\" \
    "
ninja -C out/static
du -hs out/static/libskia.a

cd ..
