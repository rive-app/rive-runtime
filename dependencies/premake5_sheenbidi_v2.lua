dofile('rive_build_config.lua')

local dependency = require('dependency')
sheenbidi = dependency.github('Tehreer/SheenBidi', 'v2.6')

project('rive_sheenbidi')
do
    kind('StaticLib')
    language('C')
    warnings('Off')

    includedirs({ sheenbidi .. '/Headers' })

    buildoptions({ '-Wall', '-ansi', '-pedantic' })

    linkoptions({ '-r' })

    filter('options:config=debug')
    do
        files({
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
            sheenbidi .. '/Source/StatusStack.c',
        })
    end
    filter('options:config=release')
    do
        files({ sheenbidi .. '/Source/SheenBidi.c' })
    end

    filter('options:config=release')
    do
        defines({ 'SB_CONFIG_UNITY' })
        optimize('Size')
    end
end
