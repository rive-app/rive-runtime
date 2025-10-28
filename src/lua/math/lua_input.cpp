
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "lualib.h"

using namespace rive;

static int pointer_event_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto pointerEvent = lua_torive<ScriptedPointerEvent>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::id:
            lua_pushunsigned(L, (unsigned)pointerEvent->m_id);
            return 1;
        case (int)LuaAtoms::position:
            lua_pushvector2(L,
                            pointerEvent->m_position.x,
                            pointerEvent->m_position.y);
            return 1;
    }

    luaL_error(L,
               "%s is not a valid field of %s",
               luaL_checkstring(L, 1),
               ScriptedPointerEvent::luaName);
    return 0;
}

static int pointer_event_hit(lua_State* L)
{
    auto pointerEvent = lua_torive<ScriptedPointerEvent>(L, 1);
    if (lua_isboolean(L, 2))
    {
        pointerEvent->m_hitResult =
            lua_toboolean(L, 2) ? HitResult::hit : HitResult::hitOpaque;
    }
    else
    {
        pointerEvent->m_hitResult = HitResult::hitOpaque;
    }
    return 0;
}

static int pointer_event_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::hit:
                return pointer_event_hit(L);
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               luaL_checkstring(L, 1),
               ScriptedPointerEvent::luaName);
    return 0;
}
int luaopen_rive_input(lua_State* L)
{
    lua_register_rive<ScriptedPointerEvent>(L);
    lua_pushcfunction(L, pointer_event_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, pointer_event_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    return 1;
}

#endif
