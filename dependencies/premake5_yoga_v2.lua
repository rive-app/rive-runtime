dofile('rive_build_config.lua')

local dependency = require('dependency')
yoga = dependency.github('rive-app/yoga', 'rive_changes_v2_0_1')

project('rive_yoga')
do
    kind('StaticLib')
    warnings('Off')

    defines({ 'YOGA_EXPORT=' })

    includedirs({ yoga })

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
end
