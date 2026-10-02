#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/transition_child_ref.hpp"

using namespace rive;

void TransitionChild::draw(Renderer* renderer)
{
    ScriptBackend::TransitionChildRef{m_artboard, m_worldTransform}.draw(
        renderer);
}

float TransitionChild::width() const
{
    return ScriptBackend::TransitionChildRef{m_artboard, m_worldTransform}
        .width();
}

float TransitionChild::height() const
{
    return ScriptBackend::TransitionChildRef{m_artboard, m_worldTransform}
        .height();
}

static int transition_child_draw(lua_State* L)
{
    auto child = lua_torive<TransitionChild>(L, 1);
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 2);
    auto renderer = scriptedRenderer->validate(L);
    child->draw(renderer);
    return 0;
}

static int transition_child_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::draw:
                return transition_child_draw(L);
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               TransitionChild::luaName);
    return 0;
}

static int transition_child_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto child = lua_torive<TransitionChild>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::width:
            lua_pushnumber(L, child->width());
            return 1;
        case (int)LuaAtoms::height:
            lua_pushnumber(L, child->height());
            return 1;
        default:
            break;
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               TransitionChild::luaName);
    return 0;
}

int luaopen_rive_transition(lua_State* L)
{
    lua_register_rive<TransitionChild>(L);

    lua_pushcfunction(L, transition_child_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, transition_child_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable
    return 0;
}

#endif
