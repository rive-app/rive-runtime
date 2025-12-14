#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_object.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

ScriptedContext::ScriptedContext(ScriptedObject* scriptedObject) :
    m_scriptedObject(scriptedObject)
{}

static int context_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto scriptedContext = lua_torive<ScriptedContext>(L, 1);
        switch (atom)
        {
            case (int)LuaAtoms::markNeedsUpdate:
            {
                auto scriptedObject = scriptedContext->scriptedObject();
                scriptedObject->markNeedsUpdate();
                return 0;
            }
            case (int)LuaAtoms::viewModel:
            {
                auto scriptedObject = scriptedContext->scriptedObject();
                auto dataContext = scriptedObject->dataContext();
                if (dataContext && dataContext->viewModelInstance())
                {
                    auto viewModelInstance = dataContext->viewModelInstance();
                    lua_newrive<ScriptedViewModel>(
                        L,
                        L,
                        ref_rcp(viewModelInstance->viewModel()),
                        viewModelInstance);
                    return 1;
                }
                return 0;
            }
            default:
                break;
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedContext::luaName);
    return 0;
}

int luaopen_rive_contex(lua_State* L)
{
    {
        lua_register_rive<ScriptedContext>(L);

        lua_pushcfunction(L, context_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }

    return 0;
}

#endif
