workspace 'rive'
configurations {'debug', 'release'}

dependencies = os.getenv('DEPENDENCIES')

rive = '../../'

dofile(path.join(path.getabsolute(rive) .. '/build', 'premake5.lua'))

project 'rive_tess_renderer'
do
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++11'
    toolset 'clang'
    targetdir '%{cfg.system}/bin/%{cfg.buildcfg}'
    objdir '%{cfg.system}/obj/%{cfg.buildcfg}'
    includedirs {
        '../include',
        rive .. '/include',
        dependencies .. '/sokol',
        dependencies .. '/earcut.hpp/include/mapbox',
        dependencies .. '/libtess2/Include'
    }
    files {
        '../src/**.cpp',
        dependencies .. '/libtess2/Source/**.c'
    }
    buildoptions {'-Wall', '-fno-exceptions', '-fno-rtti', '-Werror=format'}

    filter 'configurations:debug'
    do
        buildoptions {'-g'}
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'configurations:release'
    do
        buildoptions {'-flto=full'}
        defines {'RELEASE', 'NDEBUG'}
        optimize 'On'
    end

    filter {'options:graphics=gl'}
    do
        defines {'SOKOL_GLCORE33'}
    end

    filter {'options:graphics=metal'}
    do
        defines {'SOKOL_METAL'}
    end

    filter {'options:graphics=d3d'}
    do
        defines {'SOKOL_D3D11'}
    end

    newoption {
        trigger = 'graphics',
        value = 'gl',
        description = 'The graphics api to use.',
        allowed = {
            {'gl'},
            {'metal'},
            {'d3d'}
        }
    }
end

project 'rive_tess_tests'
do
    dependson 'rive_tess_renderer'
    dependson 'rive'
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++17'
    toolset 'clang'
    targetdir '%{cfg.system}/bin/%{cfg.buildcfg}'
    objdir '%{cfg.system}/obj/%{cfg.buildcfg}'
    includedirs {
        rive .. 'dev/test/include', -- for catch.hpp
        rive .. 'test', -- for things like rive_file_reader.hpp
        '../include',
        rive .. '/include',
        dependencies .. '/sokol',
        dependencies .. '/earcut.hpp/include/mapbox'
    }
    files {
        '../test/**.cpp',
        rive .. 'utils/no_op_factory.cpp'
    }
    links {
        'rive_tess_renderer',
        'rive',
        'rive_harfbuzz',
        'rive_sheenbidi'
    }
    buildoptions {'-Wall', '-fno-exceptions', '-fno-rtti', '-Werror=format'}
    defines {'TESTING'}

    filter 'configurations:debug'
    do
        buildoptions {'-g'}
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'configurations:release'
    do
        buildoptions {'-flto=full'}
        defines {'RELEASE', 'NDEBUG'}
        optimize 'On'
    end
end
