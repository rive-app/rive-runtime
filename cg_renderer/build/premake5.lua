workspace 'rive'
configurations {'debug', 'release'}

require 'setup_compiler'

dependencies = os.getenv('DEPENDENCIES')

project 'rive_cg_renderer'
do
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++17'
    targetdir '%{cfg.system}/bin/%{cfg.buildcfg}'
    objdir '%{cfg.system}/obj/%{cfg.buildcfg}'
    includedirs {
        '../include',
        '../../include',
    }

    libdirs {'../../../build/%{cfg.system}/bin/%{cfg.buildcfg}'}

    files {
        '../src/**.cpp',
    }

    flags {
        'FatalCompileWarnings',
    }

    filter "system:windows"
    do
        architecture 'x64'
        defines {'_USE_MATH_DEFINES'}
    end

    filter {'system:macosx', 'options:variant=runtime'}
    do
        buildoptions {
            '-fembed-bitcode -arch arm64 -arch x86_64',
        }
    end

    if os.host() == 'macosx' then
        iphoneos_sysroot = os.outputof('xcrun --sdk iphoneos --show-sdk-path')
        iphonesimulator_sysroot = os.outputof('xcrun --sdk iphonesimulator --show-sdk-path')

        filter {'system:ios', 'options:variant=system'}
        do
            buildoptions {
                '-mios-version-min=13.0 -fembed-bitcode -arch arm64 -isysroot ' .. iphoneos_sysroot
            }
        end

        filter {'system:ios', 'options:variant=emulator'}
        do
            buildoptions {
                '--target=arm64-apple-ios13.0.0-simulator -mios-version-min=13.0 -arch x86_64 -arch arm64 -isysroot ' ..
                    iphonesimulator_sysroot
            }
            targetdir '%{cfg.system}_sim/bin/%{cfg.buildcfg}'
            objdir '%{cfg.system}_sim/obj/%{cfg.buildcfg}'
        end
    end

    filter {'configurations:release', 'system:macosx'}
    do
        buildoptions {'-flto=full'}
    end

    filter {'configurations:release', 'system:android'}
    do
        buildoptions {'-flto=full'}
    end

    filter {'configurations:release', 'system:ios'}
    do
        buildoptions {'-flto=full'}
    end

    filter 'configurations:debug'
    do
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'configurations:release'
    do
        defines {'RELEASE', 'NDEBUG'}
        optimize 'On'
    end

    filter {'options:with_rive_text'}
    do
        defines {'WITH_RIVE_TEXT'}
    end
end

newoption {
    trigger = 'with_rive_text',
    description = 'Enables text experiments'
}

newoption {
    trigger = 'variant',
    value = 'type',
    description = 'Choose a particular variant to build',
    allowed = {
        {'system', 'Builds the static library for the provided system'},
        {'emulator', 'Builds for an emulator/simulator for the provided system'},
        {'runtime', 'Build the static library specifically targeting our runtimes'}
    },
    default = 'system'
}

newoption {
    trigger = 'arch',
    value = 'ABI',
    description = 'The ABI with the right toolchain for this build, generally with Android',
    allowed = {
        {'x86'},
        {'x64'},
        {'arm'},
        {'arm64'}
    }
}
