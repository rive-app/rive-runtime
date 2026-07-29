local dependency = require('dependency')
local luau = dependency.github('luigi-rosso/luau', 'rive_0_731')
local libhydrogen = dependency.github('luigi-rosso/libhydrogen', 'rive_0_2')

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

    filter({ 'system:linux', 'options:for_unreal' })
    do
        -- Unreal loads each plugin module .so with RTLD_GLOBAL (flat namespace). Luau's
        -- fast-flag registry (Luau::FValue<T>::list) is a process-global intrusive linked
        -- list built by static initializers. When luau_vm is statically linked into more
        -- than one Rive module, these weak symbols get interposed across modules and the
        -- list is cross-linked into a cycle, so FValueVersionSetter's strcmp walk spins
        -- forever during dlopen
        buildoptions({ '-fvisibility=hidden', '-fvisibility-inlines-hidden' })
    end
    filter({})
    if TESTING == true then
        filter({ 'system:windows' })
        do
            -- Visual Studio compiles through clang-cl and takes MSVC flag syntax; ninja drives
            -- clang++ directly, which only understands the GNU spelling.
            buildoptions({ _ACTION == 'ninja' and '-ffp-model=precise' or '/fp:precise' })
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
            luau .. '/Bytecode/include',
            luau .. '/Ast/include',
            luau .. '/Common/include',
        })

        files({
            luau .. '/Compiler/src/**.cpp',
            luau .. '/Bytecode/src/**.cpp',
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
