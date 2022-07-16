dependencies = os.getenv('DEPENDENCIES')

libpng = dependencies .. '/libpng'

project 'libpng'
do
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++17'
    toolset 'clang'
    targetdir '%{cfg.system}/bin/%{cfg.buildcfg}/'
    objdir '%{cfg.system}/obj/%{cfg.buildcfg}/'
    buildoptions {
        '-fno-exceptions',
        '-fno-rtti'
    }
    includedirs {
        '../include/viewer/tess',
        dependencies,
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

    architecture('ARM64')
    do
        files {
            libpng .. '/arm/arm_init.c',
            libpng .. '/arm/filter_neon_intrinsics.c',
            libpng .. '/arm/palette_neon_intrinsics.c'
        }
    end
end

zlib = dependencies .. '/zlib'

project 'zlib'
do
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++17'
    toolset 'clang'
    targetdir '%{cfg.system}/bin/%{cfg.buildcfg}/'
    objdir '%{cfg.system}/obj/%{cfg.buildcfg}/'
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
