dofile('rive_build_config.lua')
local dependency = require('dependency')
miniaudio = dependency.github('rive-app/miniaudio', 'rive_changes_5')

project('miniaudio')
do
    kind('StaticLib')
    includedirs({ miniaudio })

    filter('system:ios')
    do
        files({ 'miniaudio.m' })
    end
    filter('system:not ios')
    do
        files({ miniaudio .. '/miniaudio.c' })
    end
end
