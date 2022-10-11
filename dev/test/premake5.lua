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
    cppdialect 'C++17'
    toolset 'clang'
    targetdir 'build/bin/%{cfg.buildcfg}'
    objdir 'build/obj/%{cfg.buildcfg}'

    buildoptions {'-Wall', '-fno-exceptions', '-fno-rtti'}

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
        flags {'FatalWarnings'}
        removebuildoptions {
            -- vs clang doesn't recognize these on windows
            '-fno-exceptions',
            '-fno-rtti'
        }
        architecture 'x64'
        defines {
            '_USE_MATH_DEFINES',
            '_CRT_SECURE_NO_WARNINGS'
        }
        buildoptions {
            '-Wno-c++98-compat',
            '-Wno-c++98-compat-pedantic',
            '-Wno-c99-extensions',
            '-Wno-ctad-maybe-unsupported',
            '-Wno-deprecated-copy-with-user-provided-dtor',
            '-Wno-deprecated-declarations',
            '-Wno-documentation',
            '-Wno-documentation-pedantic',
            '-Wno-documentation-unknown-command',
            '-Wno-double-promotion',
            '-Wno-exit-time-destructors',
            '-Wno-float-equal',
            '-Wno-global-constructors',
            '-Wno-implicit-float-conversion',
            '-Wno-newline-eof',
            '-Wno-old-style-cast',
            '-Wno-reserved-identifier',
            '-Wno-shadow',
            '-Wno-sign-compare',
            '-Wno-sign-conversion',
            '-Wno-unused-macros',
            '-Wno-unused-parameter',
            '-Wno-four-char-constants',
            '-Wno-unreachable-code',
            '-Wno-switch-enum',
            '-Wno-missing-field-initializers'
        }
    end
end
