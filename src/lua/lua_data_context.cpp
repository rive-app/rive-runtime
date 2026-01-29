#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

ScriptedDataContext::ScriptedDataContext(lua_State* L,
                                         rcp<DataContext> dataContext) :
    m_state(L), m_dataContext(dataContext)
{}

int ScriptedDataContext::pushParent()
{
    if (m_dataContext->parent())
    {

        lua_newrive<ScriptedDataContext>(m_state,
                                         m_state,
                                         m_dataContext->parent());
    }
    else
    {
        lua_pushnil(m_state);
    }
    return 1;
}

int ScriptedDataContext::pushViewModel()
{
    auto vmi = m_dataContext->viewModelInstance();
    if (vmi)
    {
        lua_newrive<ScriptedViewModel>(m_state,
                                       m_state,
                                       ref_rcp(vmi->viewModel()),
                                       vmi);
    }
    else
    {
        lua_pushnil(m_state);
    }
    return 1;
}

static int data_value_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto dataContext = lua_torive<ScriptedDataContext>(L, 1);
        switch (atom)
        {
            case (int)LuaAtoms::parent:
            {
                assert(dataContext->state() == L);
                return dataContext->pushParent();
            }
            case (int)LuaAtoms::viewModel:
            {
                assert(dataContext->state() == L);
                return dataContext->pushViewModel();
            }
            default:
                break;
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedDataContext::luaName);
    return 0;
}

int luaopen_rive_data_context(lua_State* L)
{
    {
        lua_register_rive<ScriptedDataContext>(L);

        lua_pushcfunction(L, data_value_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }

    return 1;
}
#endif
