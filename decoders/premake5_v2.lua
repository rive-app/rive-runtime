dofile('rive_build_config.lua')

if _OPTIONS['no-rive-decoders'] then
    return
end

rive = path.getabsolute('../')

dofile(rive .. '/dependencies/premake5_libpng_v2.lua')
dofile(rive .. '/dependencies/premake5_libjpeg_v2.lua')
dofile(rive .. '/dependencies/premake5_libwebp_v2.lua')

newoption({
    trigger = 'no_rive_png',
    description = 'don\'t build png (or zlib) support into the rive_decoders library (built-in png decoding will fail)',
})

newoption({
    trigger = 'no_rive_jpeg',
    description = 'don\'t build jpeg support into the rive_decoders library (built-in jpeg decoding will fail)',
})

project('rive_decoders')
do
    dependson('libwebp')
    kind('StaticLib')
    flags({ 'FatalCompileWarnings' })

    includedirs({ 'include', '../include', libpng, libjpeg, libwebp .. '/src' })

    files({ 'src/bitmap_decoder.cpp' })

    filter({ 'options:not no-libjpeg-renames' })
    do
        includedirs({
            rive .. '/dependencies',
        })
        forceincludes({ 'rive_libjpeg_renames.h' })
    end

    filter({ 'system:macosx or system:ios', 'not options:variant=appletvos', 'not options:variant=appletvsimulator' })
    do
        files({ 'src/**.mm' })
    end

    filter({ 'system:ios' , 'options:variant=appletvos or options:variant=appletvsimulator' })
    do
        files({
            'src/bitmap_decoder_thirdparty.cpp',
            'src/decode_webp.cpp',
            'src/decode_jpeg.cpp',
            'src/decode_png.cpp',
        })
    end

    filter({ 'system:not macosx', 'system:not ios' })
    do
        files({
            'src/bitmap_decoder_thirdparty.cpp',
            'src/decode_webp.cpp',
        })
    end

    filter({ 'system:not macosx', 'system:not ios', 'options:not no_rive_png' })
    do
        dependson('zlib', 'libpng')
        defines({ 'RIVE_PNG' })
        files({ 'src/decode_png.cpp' })
    end

    filter({ 'system:not macosx', 'system:not ios', 'options:not no_rive_jpeg' })
    do
        dependson('libjpeg')
        defines({ 'RIVE_JPEG' })
        files({ 'src/decode_jpeg.cpp' })
    end
end
