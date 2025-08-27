#ifdef WITH_RIVE_SCRIPTING
#include "lualib.h"
#include "rive/math/vec2d.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/factory.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

static int vertex_buffer_construct(lua_State* L)
{
    lua_newrive<ScriptedVertexBuffer>(L);
    return 1;
}

static int vertex_buffer_reset(lua_State* L)
{
    auto vertexBuffer = lua_torive<ScriptedVertexBuffer>(L, 1);
    vertexBuffer->values.clear();
    vertexBuffer->vertexBuffer = nullptr;
    return 0;
}

static int vertex_buffer_add(lua_State* L)
{
    auto vertexBuffer = lua_torive<ScriptedVertexBuffer>(L, 1);
    int nargs = lua_gettop(L) - 1;
    for (int i = 0; i < nargs; i++)
    {
        auto vec = lua_checkvec2d(L, 2 + i);
        vertexBuffer->values.push_back(*vec);
    }
    vertexBuffer->vertexBuffer = nullptr;
    return 0;
}

static int vertex_buffer_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::reset:
                return vertex_buffer_reset(L);
            case (int)LuaAtoms::add:
                return vertex_buffer_add(L);
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedVertexBuffer::luaName);
    return 0;
}

static int index_buffer_construct(lua_State* L)
{
    lua_newrive<ScriptedTriangleBuffer>(L);
    return 1;
}

static int index_buffer_reset(lua_State* L)
{
    auto indexBuffer = lua_torive<ScriptedTriangleBuffer>(L, 1);
    indexBuffer->values.clear();
    indexBuffer->max = 0;
    indexBuffer->indexBuffer = nullptr;
    return 0;
}

static int index_buffer_add(lua_State* L)
{
    auto indexBuffer = lua_torive<ScriptedTriangleBuffer>(L, 1);
    for (int i = 0; i < 3; i++)
    {
        auto index = luaL_checkunsigned(L, 2 + i);
        if (index > std::numeric_limits<uint16_t>::max())
        {
            luaL_error(L,
                       "index %i exceeds %i",
                       index,
                       std::numeric_limits<uint16_t>::max());
        }
        if (index > indexBuffer->max)
        {
            indexBuffer->max = index;
        }
        indexBuffer->values.push_back((uint16_t)index);
    }
    indexBuffer->indexBuffer = nullptr;

    return 0;
}

static int index_buffer_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::reset:
                return index_buffer_reset(L);
            case (int)LuaAtoms::add:
                return index_buffer_add(L);
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedTriangleBuffer::luaName);
    return 0;
}

static const luaL_Reg empty[] = {
    {NULL, NULL},
};

static void register_vertex_buffer(lua_State* L)
{
    luaL_register(L, ScriptedVertexBuffer::luaName, empty);
    lua_register_rive<ScriptedVertexBuffer>(L);

    lua_pushcfunction(L, vertex_buffer_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    // Create metatable for the metatable (so we can call it).
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, vertex_buffer_construct, nullptr);
    lua_setfield(L, -2, "__call");
    // -3 as it's the library (Path) that we're setting this metatable on.
    lua_setmetatable(L, -3);

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable
}

static void register_index_buffer(lua_State* L)
{
    luaL_register(L, ScriptedTriangleBuffer::luaName, empty);
    lua_register_rive<ScriptedTriangleBuffer>(L);

    lua_pushcfunction(L, index_buffer_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    // Create metatable for the metatable (so we can call it).
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, index_buffer_construct, nullptr);
    lua_setfield(L, -2, "__call");
    // -3 as it's the library (Path) that we're setting this metatable on.
    lua_setmetatable(L, -3);

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable
}

void ScriptedVertexBuffer::update(Factory* factory)
{
    // still valid?
    if (vertexBuffer)
    {
        return;
    }
    auto buffer =
        factory->makeRenderBuffer(RenderBufferType::vertex,
                                  RenderBufferFlags::mappedOnceAtInitialization,
                                  values.size() * sizeof(Vec2D));
    if (buffer)
    {
        float* pos = static_cast<float*>(buffer->map());
        memcpy(pos, values.data(), buffer->sizeInBytes());
        buffer->unmap();
    }
    vertexBuffer = buffer;
}

void ScriptedTriangleBuffer::update(Factory* factory)
{
    // still valid?
    if (indexBuffer)
    {
        return;
    }
    auto buffer =
        factory->makeRenderBuffer(RenderBufferType::index,
                                  RenderBufferFlags::mappedOnceAtInitialization,
                                  values.size() * sizeof(uint16_t));
    if (buffer)
    {
        void* indexData = buffer->map();
        memcpy(indexData, values.data(), buffer->sizeInBytes());
        buffer->unmap();
    }
    indexBuffer = buffer;
}

int luaopen_rive_mesh(lua_State* L)
{
    register_vertex_buffer(L);
    register_index_buffer(L);

    return 2;
}

#endif
