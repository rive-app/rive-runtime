-- require "lfs"
-- Clean Function --
newaction {
    trigger = 'clean',
    description = 'clean the build',
    execute = function()
        print('clean the build...')
        os.rmdir('build')
        os.remove('Makefile')
        -- no wildcards in os.remove, so use shell
        os.execute('rm *.make')
        print('build cleaned')
    end
}

workspace 'rive'
configurations {'debug'}

dofile(path.join(path.getabsolute('../../dependencies/'), 'premake5_harfbuzz.lua'))
dofile(path.join(path.getabsolute('../../dependencies/'), 'premake5_sheenbidi.lua'))

project('tests')
do
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++11'
    targetdir 'build/bin/%{cfg.buildcfg}'
    objdir 'build/obj/%{cfg.buildcfg}'
    flags {'FatalWarnings'}
    buildoptions {'-Wall', '-fno-exceptions', '-fno-rtti'}
    exceptionhandling "On"

    includedirs {
        './include',
        '../../include',
        harfbuzz .. '/src',
        sheenbidi .. '/Headers'
    }
    links {
        'rive_harfbuzz',
        'rive_sheenbidi'
    }

    files {
        '../../src/**.cpp', -- the Rive runtime source
        '../../test/**.cpp', -- the tests
        '../../utils/**.cpp' -- no_op utils
    }

    defines {'TESTING', 'ENABLE_QUERY_FLAT_VERTICES', 'WITH_RIVE_TOOLS', 'WITH_RIVE_TEXT'}

    filter 'configurations:debug'
    do
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'system:windows'
    do
        removebuildoptions {
            -- vs clang doesn't recognize these on windows
            '-fno-exceptions',
            '-fno-rtti'
        }
        architecture 'x64'
        defines {
            '_USE_MATH_DEFINES',
            '_CRT_SECURE_NO_WARNINGS',
            '_CRT_NONSTDC_NO_DEPRECATE'
        }
    end
end
