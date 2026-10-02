#if defined(WITH_RIVE_SCRIPTING) && defined(WITH_RIVE_SCRIPTNET)

#include "rive/lua/rive_lua_libs.hpp"
#include "rive/artboard.hpp"
#include "rive/bindable_artboard.hpp"
#include "rive/file.hpp"
#include "rive/scripted/decoded_file.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "lua.h"
#include "lualib.h"

using namespace rive;

ScriptedRiveFile::ScriptedRiveFile() = default;
ScriptedRiveFile::~ScriptedRiveFile() = default;

ScriptedBindableArtboard::ScriptedBindableArtboard() = default;
ScriptedBindableArtboard::~ScriptedBindableArtboard() = default;

int ScriptedBindableArtboard::pushData(lua_State* L)
{
    if (viewModel == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    if (m_dataCache.push(L, viewModel))
    {
        return 1;
    }
    lua_newrive<ScriptedViewModel>(L,
                                   L,
                                   ref_rcp(viewModel->viewModel()),
                                   viewModel);
    m_dataCache.store(L, viewModel);
    return 1;
}

namespace
{

// ── RiveFile ───────────────────────────────────────────────────────────────

int riveFileIndex(lua_State* L)
{
    auto* self = lua_torive<ScriptedRiveFile>(L, 1);
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (key == nullptr)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    switch (atom)
    {
        case (int)LuaAtoms::artboardNames:
        {
            size_t count = self->file->artboardCount();
            lua_createtable(L, (int)count, 0);
            for (size_t i = 0; i < count; i++)
            {
                const std::string& name = self->file->artboard(i)->name();
                lua_pushlstring(L, name.data(), name.size());
                lua_rawseti(L, -2, (int)i + 1);
            }
            return 1;
        }
        default:
            break;
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedRiveFile::luaName);
    return 0;
}

int riveFileNamecall(lua_State* L)
{
    int atom;
    const char* name = lua_namecallatom(L, &atom);
    auto* self = lua_torive<ScriptedRiveFile>(L, 1);
    if (name != nullptr && atom == (int)LuaAtoms::bindableArtboard)
    {
        const char* artboardName =
            lua_isnoneornil(L, 2) ? nullptr : luaL_checkstring(L, 2);
        auto* bindable = lua_newrive<ScriptedBindableArtboard>(L);
        DecodedBindable decoded = decodedBindable(*self->file, artboardName);
        if (decoded.artboard == nullptr)
        {
            lua_pop(L, 1);
            lua_pushnil(L);
            return 1;
        }
        bindable->artboard = std::move(decoded.artboard);
        bindable->viewModel = std::move(decoded.viewModel);
        return 1;
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               name != nullptr ? name : "?",
               ScriptedRiveFile::luaName);
    return 0;
}

// ── BindableArtboard ───────────────────────────────────────────────────────

int bindableArtboardIndex(lua_State* L)
{
    auto* self = lua_torive<ScriptedBindableArtboard>(L, 1);
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (key == nullptr)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    switch (atom)
    {
        case (int)LuaAtoms::name:
        {
            const std::string& name = self->artboard->artboard()->name();
            lua_pushlstring(L, name.data(), name.size());
            return 1;
        }
        case (int)LuaAtoms::data:
            return self->pushData(L);
        default:
            break;
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedBindableArtboard::luaName);
    return 0;
}

// Wrappers are minted per read, so equality is by the artboard they carry.
int bindableArtboardEq(lua_State* L)
{
    auto* lhs = lua_torive<ScriptedBindableArtboard>(L, 1);
    auto* rhs = lua_torive<ScriptedBindableArtboard>(L, 2);
    lua_pushboolean(L, lhs->artboard == rhs->artboard);
    return 1;
}
} // namespace

// ── context:decodeFile(data) ───────────────────────────────────────────────

// Decodes on the calling thread: import calls into the Factory and runs the
// file's module bodies, neither of which is safe on a worker. The returned
// Promise is already settled, which keeps the signature open to an
// off-thread decode later.
int context_decodeFile_impl(lua_State* L)
{
    // Luau errors longjmp past C++ destructors, so every check that can raise
    // runs before anything is decoded.
    size_t length = 0;
    const void* data = nullptr;
    if (lua_isbuffer(L, 2))
    {
        data = lua_tobuffer(L, 2, &length);
    }
    else
    {
        luaL_typeerror(L, 2, "buffer");
        return 0;
    }
    if (data == nullptr || length == 0)
    {
        luaL_error(L, "decodeFile: empty buffer");
        return 0;
    }
    ScriptingContext* context = ScriptingContext::from(L);
    Factory* factory = context != nullptr ? context->factory() : nullptr;
    if (factory == nullptr)
    {
        luaL_error(L,
                   "decodeFile: this Lua state has no factory to decode "
                   "with");
        return 0;
    }

    auto* promise = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
    int promiseIdx = lua_gettop(L);
    // Allocated before the import so no allocation can fail while the decoded
    // file is held only by a C++ local.
    auto* riveFile = lua_newrive<ScriptedRiveFile>(L);
    int fileIdx = lua_gettop(L);

    ImportResult result = ImportResult::malformed;
    riveFile->file = importDecodedFile(
        Span<const uint8_t>(static_cast<const uint8_t*>(data), length),
        factory,
        &result);
    if (riveFile->file != nullptr)
    {
        promise->resolve(L, fileIdx);
    }
    else
    {
        lua_pushstring(L,
                       result == ImportResult::unsupportedVersion
                           ? "unsupportedVersion: not a Rive file this "
                             "runtime can read"
                           : "malformed: not a Rive file");
        promise->reject(L, lua_gettop(L));
        lua_pop(L, 1);
    }
    lua_pushvalue(L, promiseIdx);
    return 1;
}

int luaopen_rive_file(lua_State* L)
{
    {
        lua_register_rive<ScriptedRiveFile>(L);

        lua_pushcfunction(L, riveFileIndex, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, riveFileNamecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    {
        lua_register_rive<ScriptedBindableArtboard>(L);

        lua_pushcfunction(L, bindableArtboardIndex, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, bindableArtboardEq, nullptr);
        lua_setfield(L, -2, "__eq");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    return 0;
}

#endif
