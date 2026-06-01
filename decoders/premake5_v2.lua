dofile('rive_build_config.lua')

if _OPTIONS['no-rive-decoders'] then
    return
end

local rive = path.getabsolute('../')

newoption({
    trigger = 'no_rive_png',
    description = 'don\'t build png (or zlib) support into the rive_decoders library (built-in png decoding will fail)',
})

newoption({
    trigger = 'no_rive_jpeg',
    description = 'don\'t build jpeg support into the rive_decoders library (built-in jpeg decoding will fail)',
})

newoption({
    trigger = 'no_rive_webp',
    description = 'don\'t build webp support into the rive_decoders library (built-in webp decoding will fail)',
})

newoption({
    trigger = 'no_rive_ktx2',
    description = 'don\'t build KTX2 container parsing into the rive_decoders library',
})

newoption({
    trigger = 'with_rive_astc_decoder',
    description = 'build ASTC software decoder into the rive_decoders library (requires astcenc)',
})

newoption({
    trigger = 'with_rive_bc_decoder',
    description = 'build BCn software decoder into the rive_decoders library (requires bc7enc_rdo)',
})

newoption({
    trigger = 'with_rive_etc_decoder',
    description = 'build ETC2 software decoder into the rive_decoders library (requires Ericsson ETCPACK)',
})

if not _OPTIONS["no_rive_png"] then
    dofile(rive .. '/dependencies/premake5_libpng_v2.lua')
end

if not _OPTIONS["no_rive_jpeg"] then
    dofile(rive .. '/dependencies/premake5_libjpeg_v2.lua')
end

if not _OPTIONS["no_rive_webp"] then
    dofile(rive .. '/dependencies/premake5_libwebp_v2.lua')
else
    libwebp = ''
end

local dependency = require('dependency')

if _OPTIONS["with_rive_astc_decoder"] then
    astcenc = dependency.github('ARM-software/astc-encoder', '4.7.0')
end

if _OPTIONS["with_rive_bc_decoder"] then
    bc7enc = dependency.github('richgel999/bc7enc_rdo', 'master')
end

if _OPTIONS["with_rive_etc_decoder"] then
    etcpack = dependency.github('Ericsson/ETCPACK', 'master')
end

project('rive_decoders')
do
    kind('StaticLib')
    fatalwarnings { "All" }

    includedirs({
        'include',
        '../include',
        '%{cfg.targetdir}/include/libpng',
    })

    files({
        'src/bitmap_decoder.cpp',
    })

    filter({ 'options:not no-libjpeg-renames' })
    do
        includedirs({
            rive .. '/dependencies',
        })
        forceincludes({ 'rive_libjpeg_renames.h' })
    end

    filter({
        'system:macosx or system:ios',
        'not options:variant=appletvos',
        'not options:variant=appletvsimulator',
    })
    do
        files({ 'src/**.mm' })
    end

    filter({ 'options:not no_rive_webp or no_rive_png or no_rive_jpeg'})
    do
        files({
            'src/bitmap_decoder_thirdparty.cpp',
        })
    end

    filter({'options:not no_rive_png'})
    do
        includedirs({
            libpng
            })
        dependson('zlib', 'libpng')
        defines({ 'RIVE_PNG' })
        files({ 'src/decode_png.cpp' })
    end

    filter({ 'options:not no_rive_jpeg' })
    do
        includedirs({
            libjpeg
            })
        dependson('libjpeg')
        defines({ 'RIVE_JPEG' })
        files({ 'src/decode_jpeg.cpp' })
    end

    filter({ 'options:not no_rive_webp' })
    do
        includedirs({
            libwebp .. '/src'
            })
        dependson('libwebp')
        defines({ 'RIVE_WEBP' })
        files({ 'src/decode_webp.cpp' })
    end


    filter({ 'options:not no_rive_ktx2' })
    do
        defines({ 'RIVE_KTX2' })
        files({ 'src/decode_ktx2.cpp' })
	end
	
    -- Always include the texture decoder dispatcher; it compiles cleanly with
    -- no decoder flags set (all paths return nullptr with a log message).
    filter({})
    do
        files({ 'src/texture_decoder.cpp' })
    end

    if _OPTIONS["with_rive_astc_decoder"] then
        filter({ 'options:with_rive_astc_decoder' })
        do
            includedirs({ astcenc .. '/Source' })
            defines({
                'RIVE_ASTC_DECODER',
                'ASTCENC_SSE=0',
                'ASTCENC_POPCNT=0',
                'ASTCENC_F16C=0',
                'ASTCENC_AVX=0',
                'ASTCENC_NEON=0',
            })
            files({
                'src/decode_astc_texture.cpp',
                astcenc .. '/Source/astcenc_*.cpp',
            })
            buildoptions({
                '-Wno-sign-conversion',
                '-Wno-implicit-int-float-conversion',
                '-Wno-float-conversion',
                '-Wno-shorten-64-to-32',
                '-Wno-unused-variable',
                '-Wno-unused-function',
                '-Wno-shadow',
                '-Wno-missing-field-initializers',
            })
        end
    end

    if _OPTIONS["with_rive_bc_decoder"] then
        filter({ 'options:with_rive_bc_decoder' })
        do
            includedirs({ bc7enc })
            defines({ 'RIVE_BC_DECODER' })
            files({
                'src/decode_bc_texture.cpp',
                bc7enc .. '/bc7decomp.cpp',
                bc7enc .. '/bc7decomp_ref.cpp',
                bc7enc .. '/rgbcx.cpp',
            })
            buildoptions({
                '-Wno-sign-conversion',
                '-Wno-implicit-int-float-conversion',
                '-Wno-float-conversion',
                '-Wno-shorten-64-to-32',
                '-Wno-unused-variable',
                '-Wno-unused-function',
                '-Wno-unused-const-variable',
                '-Wno-shadow',
                '-Wno-missing-field-initializers',
            })
        end
    end

    if _OPTIONS["with_rive_etc_decoder"] then
        filter({ 'options:with_rive_etc_decoder' })
        do
            defines({ 'RIVE_ETC_DECODER' })
            files({
                'src/decode_etc_texture.cpp',
                etcpack .. '/source/etcdec.cxx',
            })
            buildoptions({
                '-Wno-sign-conversion',
                '-Wno-implicit-int-float-conversion',
                '-Wno-float-conversion',
                '-Wno-shorten-64-to-32',
                '-Wno-unused-variable',
                '-Wno-unused-function',
                '-Wno-unused-but-set-variable',
                '-Wno-shadow',
                '-Wno-missing-field-initializers',
                '-Wno-old-style-cast',
                '-Wno-parentheses',
                '-Wno-sign-compare',
            })
        end
    end

end
