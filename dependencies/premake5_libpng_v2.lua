dofile('rive_build_config.lua')

local dependency = require('dependency')
libpng = dependency.github('glennrp/libpng', 'libpng16')
zlib = dependency.github('madler/zlib', 'v1.3.1')

includedirs({ './' })
forceincludes({ 'rive_png_renames.h' })

-- Generate a pnglibconf.h in our OUT directory.
if not os.isdir(RIVE_BUILD_OUT .. '/include/libpng') then
    os.mkdir(RIVE_BUILD_OUT .. '/include/libpng')
    os.copyfile(
        libpng .. '/scripts/pnglibconf.h.prebuilt',
        RIVE_BUILD_OUT .. '/include/libpng/pnglibconf.h'
    )
end

project('libpng')
do
    kind('StaticLib')
    optimize('Speed') -- Always optimize image encoding/decoding, even in debug builds.
    includedirs({ libpng, zlib, '%{cfg.targetdir}/include/libpng' })
    files({
        libpng .. '/png.c',
        libpng .. '/pngerror.c',
        libpng .. '/pngget.c',
        libpng .. '/pngmem.c',
        libpng .. '/pngpread.c',
        libpng .. '/pngread.c',
        libpng .. '/pngrio.c',
        libpng .. '/pngrtran.c',
        libpng .. '/pngrutil.c',
        libpng .. '/pngset.c',
        libpng .. '/pngtrans.c',
        libpng .. '/pngwio.c',
        libpng .. '/pngwrite.c',
        libpng .. '/pngwtran.c',
        libpng .. '/pngwutil.c',
    })

    do
        files({
            libpng .. '/arm/arm_init.c',
            libpng .. '/arm/filter_neon_intrinsics.c',
            libpng .. '/arm/palette_neon_intrinsics.c',
        })
    end
end

newoption({
    trigger = 'force_no_unistd_h',
    description = 'prevent HAVE_UNISTD_H from being defined for zlib',
})

project('zlib')
do
    kind('StaticLib')
    defines({ 'ZLIB_IMPLEMENTATION' })

    includedirs({ zlib })
    optimize('Speed') -- Always optimize image encoding/decoding, even in debug builds.
    files({
        zlib .. '/adler32.c',
        zlib .. '/compress.c',
        zlib .. '/crc32.c',
        zlib .. '/deflate.c',
        zlib .. '/gzclose.c',
        zlib .. '/gzlib.c',
        zlib .. '/gzread.c',
        zlib .. '/gzwrite.c',
        zlib .. '/infback.c',
        zlib .. '/inffast.c',
        zlib .. '/inftrees.c',
        zlib .. '/trees.c',
        zlib .. '/uncompr.c',
        zlib .. '/zutil.c',
        zlib .. '/inflate.c',
    })

    filter('toolset:not msc')
    do
        fatalwarnings { "All" }
        buildoptions({
            '-Wno-unknown-warning-option',
            '-Wno-deprecated-non-prototype',
            '-Wno-shorten-64-to-32',
        })
    end

    filter({ 'system:not windows', 'options:not force_no_unistd_h' })
    do
        defines({ 'HAVE_UNISTD_H' })
    end
end
