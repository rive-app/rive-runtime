dofile('rive_build_config.lua')

local dependency = require('dependency')
yoga = dependency.github('rive-app/yoga', 'rive_changes_v2_0_1')

newoption({
    trigger = 'no-yoga-renames',
    description = 'don\'t rename yoga symbols',
})

project('rive_yoga')
do
    kind('StaticLib')
    warnings('Off')

    defines({ 'YOGA_EXPORT=' })

    includedirs({ yoga })

    filter('action:xcode4')
    do
        -- xcode doesnt like angle brackets except for -isystem
        -- should use externalincludedirs but GitHub runners dont have latest premake5 binaries
        buildoptions({ '-isystem' .. yoga })
    end
    filter({})

    files({
        yoga .. '/yoga/Utils.cpp',
        yoga .. '/yoga/YGConfig.cpp',
        yoga .. '/yoga/YGLayout.cpp',
        yoga .. '/yoga/YGEnums.cpp',
        yoga .. '/yoga/YGNodePrint.cpp',
        yoga .. '/yoga/YGNode.cpp',
        yoga .. '/yoga/YGValue.cpp',
        yoga .. '/yoga/YGStyle.cpp',
        yoga .. '/yoga/Yoga.cpp',
        yoga .. '/yoga/event/event.cpp',
        yoga .. '/yoga/log.cpp',
    })

    filter({ 'options:not no-yoga-renames' })
    do
        includedirs({ './' })
        forceincludes({ 'rive_yoga_renames.h' })
    end
end
