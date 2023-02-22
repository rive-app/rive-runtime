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
        architecture 'x64'
    end
end
