local dependency = require('dependency')
local luau = dependency.github('luigi-rosso/luau', 'rive_0_8')

dofile('rive_build_config.lua')

includedirs({
    './',
})
forceincludes({ 'rive_luau.hpp' })

project('luau_vm')
do
    kind('StaticLib')

    includedirs({
        luau .. '/VM/include',
        luau .. '/Common/include',
    })

    files({
        luau .. '/VM/src/**.cpp',
    })
    defines({ 'LUA_USE_LONGJMP' })
    optimize('Size')
    if TESTING == true then
        filter({ 'system:windows' })
        do
            buildoptions({ '/fp:precise' })
        end
    end
end

project('luau_compiler')
do
    kind('StaticLib')
    exceptionhandling('On')

    includedirs({
        luau .. '/Compiler/include',
        luau .. '/Ast/include',
        luau .. '/Common/include',
    })

    files({ luau .. '/Compiler/src/**.cpp', luau .. '/Ast/src/**.cpp' })
    optimize('Size')
end

return { luau = luau }
