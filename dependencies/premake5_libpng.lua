local dependency = require 'dependency'
libpng = dependency.github('glennrp/libpng', 'libpng16')

project 'libpng'
do
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++17'
    toolset 'clang'
    targetdir '%{cfg.system}/cache/bin/%{cfg.buildcfg}/'
    objdir '%{cfg.system}/cache/obj/%{cfg.buildcfg}/'
    buildoptions {
        '-fno-exceptions',
        '-fno-rtti'
    }
    prebuildcommands {'{COPY} ' .. libpng .. '/scripts/pnglibconf.h.prebuilt ' .. libpng .. '/pnglibconf.h'}
    includedirs {
        './',
        libpng
    }
    files {
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
        libpng .. '/pngwutil.c'
    }

    do
        files {
            libpng .. '/arm/arm_init.c',
            libpng .. '/arm/filter_neon_intrinsics.c',
            libpng .. '/arm/palette_neon_intrinsics.c'
        }
    end
end

zlib = dependency.github('madler/zlib', '04f42ceca40f73e2978b50e93806c2a18c1281fc')

project 'zlib'
do
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++17'
    toolset 'clang'
    targetdir '%{cfg.system}/cache/bin/%{cfg.buildcfg}/'
    objdir '%{cfg.system}/cache/obj/%{cfg.buildcfg}/'
    buildoptions {
        '-fno-exceptions',
        '-fno-rtti'
    }
    defines {'ZLIB_IMPLEMENTATION', 'HAVE_UNISTD_H'}
    includedirs {
        zlib
    }
    files {
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
        zlib .. '/inflate.c'
    }
end
