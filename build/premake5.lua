workspace 'rive'
configurations {'debug', 'release'}

require 'setup_compiler'

filter {'options:with_rive_tools'}
do
    defines {'WITH_RIVE_TOOLS'}
end
filter {'options:with_rive_text'}
do
    defines {'WITH_RIVE_TEXT'}
end
filter {}

dofile(path.join(path.getabsolute('../dependencies/'), 'premake5_harfbuzz.lua'))
dofile(path.join(path.getabsolute('../dependencies/'), 'premake5_sheenbidi.lua'))

project 'rive'
do
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++11'
    targetdir '%{cfg.system}/bin/%{cfg.buildcfg}'
    objdir '%{cfg.system}/obj/%{cfg.buildcfg}'
    includedirs {
        '../include',
        harfbuzz .. '/src',
        sheenbidi .. '/Headers'
    }

    files {'../src/**.cpp'}

    flags {
        'FatalCompileWarnings',
    }

    filter {'system:macosx'}
    do
        buildoptions {
            -- this triggers too much on linux, so just enable here for now
            '-Wimplicit-float-conversion'
        }
    end

    filter {'toolset:not msc'}
    do
        buildoptions {
            '-Wimplicit-int-conversion',
        }
    end
    filter {'system:macosx', 'options:variant=runtime'}
    do
        buildoptions {
            '-Wimplicit-float-conversion -fembed-bitcode -arch arm64 -arch x86_64 -isysroot ' ..
                (os.getenv('MACOS_SYSROOT') or '')
        }
    end

    filter {'system:macosx', 'configurations:release'}
    do
        buildoptions {'-flto=full'}
    end

    filter {'system:ios'}
    do
        buildoptions {'-flto=full'}
    end

    filter 'system:windows'
    do
        architecture 'x64'
        defines {'_USE_MATH_DEFINES'}
    end

    filter {'system:ios', 'options:variant=system'}
    do
        buildoptions {
            '-mios-version-min=13.0 -fembed-bitcode -arch arm64 -isysroot ' ..
                (os.getenv('IOS_SYSROOT') or '')
        }
    end

    filter {'system:ios', 'options:variant=emulator'}
    do
        buildoptions {
            '-mios-version-min=13.0 -arch arm64 -arch x86_64 -isysroot ' .. (os.getenv('IOS_SYSROOT') or '')
        }
        targetdir '%{cfg.system}_sim/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}_sim/obj/%{cfg.buildcfg}'
    end

    filter {'system:android', 'configurations:release'}
    do
        buildoptions {'-flto=full'}
    end

    -- Is there a way to pass 'arch' as a variable here?
    filter {'system:android', 'options:arch=x86'}
    do
        targetdir '%{cfg.system}/x86/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}/x86/obj/%{cfg.buildcfg}'
    end

    filter {'system:android', 'options:arch=x64'}
    do
        targetdir '%{cfg.system}/x64/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}/x64/obj/%{cfg.buildcfg}'
    end

    filter {'system:android', 'options:arch=arm'}
    do
        targetdir '%{cfg.system}/arm/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}/arm/obj/%{cfg.buildcfg}'
    end

    filter {'system:android', 'options:arch=arm64'}
    do
        targetdir '%{cfg.system}/arm64/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}/arm64/obj/%{cfg.buildcfg}'
    end

    filter "system:emscripten"
    do
        buildoptions {"-pthread"}
    end

    filter 'configurations:debug'
    do
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'configurations:release'
    do
        defines {'RELEASE'}
        defines {'NDEBUG'}
        optimize 'On'
    end
end

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
    trigger = 'with_rive_tools',
    description = 'Enables tools usually not necessary for runtime.'
}

newoption {
    trigger = 'with_rive_text',
    description = 'Compiles in text features.'
}
