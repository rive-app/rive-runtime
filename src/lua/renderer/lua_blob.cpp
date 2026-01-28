#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#include "lualib.h"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/assets/blob_asset.hpp"

#include <string.h>

using namespace rive;

static int blob_index(lua_State* L)
{
    auto blob = lua_torive<ScriptedBlob>(L, 1);

    int atom;
    const char* name = lua_tostringatom(L, 2, &atom);
    if (!name)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
    }

    BlobAsset* blobAsset = blob->asset ? blob->asset->as<BlobAsset>() : nullptr;
    if (blobAsset == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }

    switch (atom)
    {
        case (int)LuaAtoms::size:
            lua_pushnumber(L, static_cast<double>(blobAsset->bytes().size()));
            return 1;
        case (int)LuaAtoms::name:
            lua_pushstring(L, blobAsset->name().c_str());
            return 1;
        case (int)LuaAtoms::data:
        {
            auto bytes = blobAsset->bytes();
            if (bytes.size() > 0)
            {
                void* bufferData = lua_newbuffer(L, bytes.size());
                memcpy(bufferData, bytes.data(), bytes.size());
                return 1;
            }
            lua_pushnil(L);
            return 1;
        }
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               name,
               ScriptedBlob::luaName);
    return 0;
}

int luaopen_rive_blob(lua_State* L)
{
    lua_register_rive<ScriptedBlob>(L);

    lua_pushcfunction(L, blob_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    return 0;
}

#endif
