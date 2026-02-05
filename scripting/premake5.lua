local dependency = require('dependency')
local luau = dependency.github('luigi-rosso/luau', 'rive_0_32')
local libhydrogen = dependency.github('luigi-rosso/libhydrogen', 'rive_0_1')

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
    defines({ 'LUA_USE_LONGJMP', 'RIVE_LUAU' })
    optimize('Size')
    if TESTING == true then
        filter({ 'system:windows' })
        do
            buildoptions({ '/fp:precise' })
        end
    end

    filter({ 'options:with-asan' })
    do
        defines({ 'LUAU_ENABLE_ASAN' })
    end
end

project('luau_compiler')
do
    kind('None')

    filter({
        'options:with_rive_tools or options:with_rive_docs',
        'options:not flutter_runtime or options:with_rive_docs',
    })
    do
        kind('StaticLib')
        exceptionhandling('On')

        includedirs({
            luau .. '/Compiler/include',
            luau .. '/Ast/include',
            luau .. '/Common/include',
        })

        files({
            luau .. '/Compiler/src/**.cpp',
            luau .. '/Ast/src/**.cpp',
            luau .. '/Common/src/**.cpp',
        })
        defines({ 'RIVE_LUAU' })
        optimize('Size')
        filter({ 'options:with-asan' })
        do
            defines({ 'LUAU_ENABLE_ASAN' })
        end
    end
end

return { luau = luau, libhydrogen = libhydrogen }
