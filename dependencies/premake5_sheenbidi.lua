local dependency = require 'dependency'
sheenbidi = dependency.github('Tehreer/SheenBidi', 'v2.6')

workspace 'rive'
configurations {'debug', 'release'}

project 'rive_sheenbidi'
do
    kind 'StaticLib'
    language 'C'
    targetdir '%{cfg.system}/cache/bin/%{cfg.buildcfg}/'
    objdir '%{cfg.system}/cache/obj/%{cfg.buildcfg}/'
    warnings 'Off'

    includedirs {
        sheenbidi .. '/Headers'
    }

    buildoptions {
        '-Wall',
        '-ansi',
        '-pedantic'
    }

    linkoptions {'-r'}

    filter 'system:emscripten'
    do
        buildoptions {'-pthread'}
    end

    filter 'configurations:debug'
    do
        files {
            sheenbidi .. '/Source/BidiChain.c',
            sheenbidi .. '/Source/BidiTypeLookup.c',
            sheenbidi .. '/Source/BracketQueue.c',
            sheenbidi .. '/Source/GeneralCategoryLookup.c',
            sheenbidi .. '/Source/IsolatingRun.c',
            sheenbidi .. '/Source/LevelRun.c',
            sheenbidi .. '/Source/PairingLookup.c',
            sheenbidi .. '/Source/RunQueue.c',
            sheenbidi .. '/Source/SBAlgorithm.c',
            sheenbidi .. '/Source/SBBase.c',
            sheenbidi .. '/Source/SBCodepointSequence.c',
            sheenbidi .. '/Source/SBLine.c',
            sheenbidi .. '/Source/SBLog.c',
            sheenbidi .. '/Source/SBMirrorLocator.c',
            sheenbidi .. '/Source/SBParagraph.c',
            sheenbidi .. '/Source/SBScriptLocator.c',
            sheenbidi .. '/Source/ScriptLookup.c',
            sheenbidi .. '/Source/ScriptStack.c',
            sheenbidi .. '/Source/StatusStack.c'
        }
    end
    filter 'configurations:release'
    do
        files {
            sheenbidi .. '/Source/SheenBidi.c'
        }
    end

    filter 'configurations:debug'
    do
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'configurations:release'
    do
        buildoptions {'-Oz'}
        defines {'RELEASE', 'NDEBUG', 'SB_CONFIG_UNITY'}
        optimize 'On'
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
            '--target=arm64-apple-ios13.0.0-simulator',
            '-mios-version-min=13.0 -arch arm64 -arch x86_64 -isysroot ' .. (os.getenv('IOS_SYSROOT') or '')
        }
        targetdir '%{cfg.system}_sim/cache/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}_sim/cache/obj/%{cfg.buildcfg}'
    end

    filter {'system:android', 'configurations:release'}
    do
        buildoptions {'-flto=full'}
    end

    -- Is there a way to pass 'arch' as a variable here?
    filter {'system:android', 'options:arch=x86'}
    do
        targetdir '%{cfg.system}/cache/x86/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}/cache/x86/obj/%{cfg.buildcfg}'
    end

    filter {'system:android', 'options:arch=x64'}
    do
        targetdir '%{cfg.system}/cache/x64/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}/cache/x64/obj/%{cfg.buildcfg}'
    end

    filter {'system:android', 'options:arch=arm'}
    do
        targetdir '%{cfg.system}/cache/arm/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}/cache/arm/obj/%{cfg.buildcfg}'
    end

    filter {'system:android', 'options:arch=arm64'}
    do
        targetdir '%{cfg.system}/cache/arm64/bin/%{cfg.buildcfg}'
        objdir '%{cfg.system}/cache/arm64/obj/%{cfg.buildcfg}'
    end
end
