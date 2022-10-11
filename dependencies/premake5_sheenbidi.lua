local dependency = require 'dependency'
sheenbidi = dependency.github('Tehreer/SheenBidi', 'v2.6')

workspace 'rive'
configurations {'debug', 'release'}

project 'rive_sheenbidi'
do
    kind 'StaticLib'
    language 'C'
    toolset 'clang'
    targetdir '%{cfg.system}/cache/bin/%{cfg.buildcfg}/'
    objdir '%{cfg.system}/cache/obj/%{cfg.buildcfg}/'

    includedirs {
        sheenbidi .. '/Headers'
    }

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

    buildoptions {
        '-Wall',
        '-ansi',
        '-pedantic'
    }

    linkoptions {'-r'}

    filter 'configurations:debug'
    do
        buildoptions {'-g', '-O0'}
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'configurations:release'
    do
        buildoptions {'-Oz'}
        defines {'RELEASE', 'NDEBUG', 'SB_CONFIG_UNITY'}
        optimize 'On'
    end

    filter 'system:windows'
    do
        removebuildoptions {
            -- vs clang doesn't recognize these on windows
            '-fno-exceptions',
            '-fno-rtti'
        }
        architecture 'x64'
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
            '-Wno-used-but-marked-unused',
            '-Wno-cast-qual',
            '-Wno-unused-template',
            '-Wno-zero-as-null-pointer-constant',
            '-Wno-extra-semi',
            '-Wno-undef',
            '-Wno-comma',
            '-Wno-nonportable-system-include-path',
            '-Wno-covered-switch-default',
            '-Wno-microsoft-enum-value',
            '-Wno-deprecated-declarations'
        }
    end
end
