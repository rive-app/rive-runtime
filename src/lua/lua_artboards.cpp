#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/viewmodel/viewmodel_property_number.hpp"
#include "rive/viewmodel/viewmodel_property_trigger.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

static int artboard_draw(lua_State* L)
{
    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 2);

    auto renderer = scriptedRenderer->validate(L);
    scriptedArtboard->artboard()->draw(renderer);

    return 0;
}

static int artboard_advance(lua_State* L)
{
    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    auto seconds = float(luaL_checknumber(L, 2));

    scriptedArtboard->artboard()->advance(seconds);

    return 0;
}

static int artboard_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::draw:
                return artboard_draw(L);
            case (int)LuaAtoms::advance:
                return artboard_advance(L);
            case (int)LuaAtoms::instance:
            {
                auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
                return scriptedArtboard->instance(L);
            }
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedArtboard::luaName);
    return 0;
}

int ScriptedArtboard::pushData(lua_State* L)
{
    if (m_dataRef != 0)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_dataRef);
        return 1;
    }
    if (m_viewModelInstance == nullptr)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_newrive<ScriptedViewModel>(
            L,
            L,
            ref_rcp(m_viewModelInstance->viewModel()),
            m_viewModelInstance);
    }
    m_dataRef = lua_ref(L, -1);

    return 1;
}

int ScriptedArtboard::instance(lua_State* L)
{
    lua_newrive<ScriptedArtboard>(L, m_file, m_artboard->instance());
    return 1;
}

static int artboard_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::frameOrigin:
            lua_pushboolean(L,
                            scriptedArtboard->artboard()->frameOrigin() ? 1
                                                                        : 0);
            return 1;

        case (int)LuaAtoms::data:
            return scriptedArtboard->pushData(L);

        default:
            return 0;
    }
}

static int artboard_newindex(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::frameOrigin:
            scriptedArtboard->artboard()->frameOrigin(luaL_checkboolean(L, 3) !=
                                                      0);
        default:
            return 0;
    }

    return 0;
}

ScriptedArtboard::ScriptedArtboard(
    rcp<File> file,
    std::unique_ptr<ArtboardInstance>&& artboardInstance) :
    m_file(file),
    m_artboard(std::move(artboardInstance)),
    m_stateMachine(m_artboard->defaultStateMachine())
{
    m_viewModelInstance = m_file->createViewModelInstance(m_artboard.get());
    m_stateMachine->bindViewModelInstance(m_viewModelInstance);
}

int luaopen_rive_artboards(lua_State* L)
{
    lua_register_rive<ScriptedArtboard>(L);

    lua_pushcfunction(L, artboard_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, artboard_newindex, nullptr);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, artboard_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    return 1;
}
#endif
