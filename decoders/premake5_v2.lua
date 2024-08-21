dofile('rive_build_config.lua')

rive = path.getabsolute('../')

dofile(rive .. '/dependencies/premake5_libpng_v2.lua')
dofile(rive .. '/dependencies/premake5_libjpeg_v2.lua')

project('rive_decoders')
do
    dependson('libpng', 'zlib', 'libjpeg')
    kind('StaticLib')
    flags({ 'FatalWarnings' })

    includedirs({ 'include', '../include', libpng, libjpeg })

    files({ 'src/bitmap_decoder.cpp' })

    filter({ 'options:not no-libjpeg-renames' })
    do
        includedirs({
            rive .. '/dependencies',
        })
        forceincludes({ 'rive_libjpeg_renames.h' })
    end

    filter({ 'system:macosx or system:ios' })
    do
        files({ 'src/**.mm' })
    end

    filter({ 'system:not macosx', 'system:not ios' })
    do
        files({
            'src/bitmap_decoder_thirdparty.cpp',
            'src/decode_jpeg.cpp',
            'src/decode_png.cpp',
        })
    end
end
