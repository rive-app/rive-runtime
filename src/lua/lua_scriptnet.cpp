#if defined(WITH_RIVE_SCRIPTING) && defined(WITH_RIVE_SCRIPTNET)

#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scriptnet/net.hpp"
#include "lua.h"
#include "lualib.h"

#include <algorithm>
#include <cstring>
#include <string>

using namespace rive;

namespace
{

// Settles one script's fetch promise. Holds the VM's main lua_State and a
// registry ref to the promise until the outcome is delivered.
class LuaFetchListener : public scriptnet::FetchListener
{
public:
    LuaFetchListener(lua_State* state, int promiseRef) :
        m_state(state), m_promiseRef(promiseRef)
    {}

    void onResponse(scriptnet::HttpResponse&& response) override
    {
        lua_State* L = m_state;
        if (L == nullptr)
        {
            return;
        }
        if (ScriptedPromise* promise = pushPendingPromise(L))
        {
            lua_newrive<ScriptedHttpResponse>(L, std::move(response));
            promise->resolve(L, lua_gettop(L));
            lua_pop(L, 2);
        }
        release(L);
    }

    void onError(const scriptnet::NetError& error) override
    {
        lua_State* L = m_state;
        if (L == nullptr)
        {
            return;
        }
        if (ScriptedPromise* promise = pushPendingPromise(L))
        {
            lua_pushfstring(L,
                            "%s: %s",
                            scriptnet::netErrorCodeName(error.code),
                            error.message.c_str());
            promise->reject(L, lua_gettop(L));
            lua_pop(L, 2);
        }
        release(L);
    }

    // The script cancelled the promise, whose cancel hook releases the ref,
    // or the VM is closing and Lua must not be touched.
    void onCancel() override
    {
        m_state = nullptr;
        m_promiseRef = LUA_NOREF;
    }

private:
    // Pushes the promise if it is still pending; pushes nothing otherwise.
    ScriptedPromise* pushPendingPromise(lua_State* L)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_promiseRef);
        auto* promise = lua_torive<ScriptedPromise>(L, -1, true);
        if (promise == nullptr || !promise->isPending())
        {
            lua_pop(L, 1);
            return nullptr;
        }
        return promise;
    }

    void release(lua_State* L)
    {
        lua_unref(L, m_promiseRef);
        m_promiseRef = LUA_NOREF;
        m_state = nullptr;
    }

    lua_State* m_state;
    int m_promiseRef;
};

// ── fetch(url, options?) ───────────────────────────────────────────────────

// Luau errors longjmp (LUA_USE_LONGJMP) past C++ destructors, so every check
// that can raise runs here, before the request exists.
void checkOptions(lua_State* L, int options)
{
    lua_getfield(L, options, "method");
    if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TSTRING)
    {
        luaL_error(L, "fetch: options.method must be a string");
    }
    lua_pop(L, 1);

    lua_getfield(L, options, "headers");
    if (!lua_isnil(L, -1))
    {
        if (!lua_istable(L, -1))
        {
            luaL_error(L, "fetch: options.headers must be a table");
        }
        int headers = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, headers) != 0)
        {
            if (lua_type(L, -2) != LUA_TSTRING ||
                lua_type(L, -1) != LUA_TSTRING)
            {
                luaL_error(L,
                           "fetch: options.headers must map header names to "
                           "string values");
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, options, "body");
    if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TSTRING &&
        !lua_isbuffer(L, -1))
    {
        luaL_error(L, "fetch: options.body must be a string or a buffer");
    }
    lua_pop(L, 1);

    lua_getfield(L, options, "timeout");
    if (!lua_isnil(L, -1) &&
        (lua_type(L, -1) != LUA_TNUMBER || !(lua_tonumber(L, -1) >= 0.0)))
    {
        luaL_error(L,
                   "fetch: options.timeout must be a non-negative number of "
                   "milliseconds");
    }
    lua_pop(L, 1);
}

// Reads arguments checkOptions already validated; raises nothing. Runs after
// the promise is pushed, so whether options exist is decided up front.
void readRequest(lua_State* L, bool hasOptions, scriptnet::HttpRequest& request)
{
    size_t length = 0;
    const char* url = lua_tolstring(L, 1, &length);
    request.url.assign(url, length);
    if (!hasOptions)
    {
        return;
    }

    lua_getfield(L, 2, "method");
    if (lua_type(L, -1) == LUA_TSTRING)
    {
        const char* method = lua_tolstring(L, -1, &length);
        request.method.assign(method, length);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "headers");
    if (lua_istable(L, -1))
    {
        int headers = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, headers) != 0)
        {
            size_t nameLength = 0;
            size_t valueLength = 0;
            const char* name = lua_tolstring(L, -2, &nameLength);
            const char* value = lua_tolstring(L, -1, &valueLength);
            request.headers.push_back({std::string(name, nameLength),
                                       std::string(value, valueLength)});
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "body");
    const void* body = nullptr;
    if (lua_type(L, -1) == LUA_TSTRING)
    {
        body = lua_tolstring(L, -1, &length);
    }
    else if (lua_isbuffer(L, -1))
    {
        body = lua_tobuffer(L, -1, &length);
    }
    if (body != nullptr && length > 0)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(body);
        request.body.assign(bytes, bytes + length);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "timeout");
    if (lua_type(L, -1) == LUA_TNUMBER)
    {
        request.timeoutMs =
            (uint32_t)std::min(lua_tonumber(L, -1), 4294967295.0);
    }
    lua_pop(L, 1);
}

// The promise's onCancel hook. A request already taken for delivery keeps
// its listener, which releases the ref itself.
int fetchCancel(lua_State* L)
{
    if (scriptnet::cancel(
            (scriptnet::RequestId)lua_tounsigned(L, lua_upvalueindex(1))))
    {
        lua_unref(L, (int)lua_tointeger(L, lua_upvalueindex(2)));
    }
    return 0;
}

int scriptnetFetch(lua_State* L)
{
    luaL_checkstring(L, 1);
    bool hasOptions = !lua_isnoneornil(L, 2);
    if (hasOptions)
    {
        luaL_checktype(L, 2, LUA_TTABLE);
        checkOptions(L, 2);
    }
    ScriptingContext* context = ScriptingContext::from(L);
    if (context == nullptr)
    {
        luaL_error(L, "fetch: this Lua state has no scripting context");
    }
    uint64_t ownerId = context->acquireOwnerId();

    lua_State* mainThread = lua_mainthread(L);
    auto* promise = lua_newrive<ScriptedPromise>(L, mainThread);
    int promiseIdx = lua_gettop(L);
    lua_pushvalue(L, promiseIdx);
    int promiseRef = lua_ref(L, -1);
    lua_pop(L, 1);

    scriptnet::RequestId id;
    {
        scriptnet::HttpRequest request;
        readRequest(L, hasOptions, request);
        id = scriptnet::fetch(
            ownerId,
            std::move(request),
            make_rcp<LuaFetchListener>(mainThread, promiseRef));
    }

    lua_pushunsigned(L, id);
    lua_pushinteger(L, promiseRef);
    lua_pushcclosurek(L, fetchCancel, "fetchCancel", 2, nullptr);
    promise->m_onCancelRef = lua_ref(L, -1);
    lua_pop(L, 1);

    lua_pushvalue(L, promiseIdx);
    return 1;
}

// ── Response ───────────────────────────────────────────────────────────────

// Header names lower-cased; repeated headers joined with ", " as the Fetch
// standard's Headers object does.
void pushHeaders(lua_State* L, const scriptnet::HttpHeaders& headers)
{
    lua_createtable(L, 0, (int)headers.size());
    for (const scriptnet::HttpHeader& header : headers)
    {
        std::string name = scriptnet::lowerAscii(header.name);
        lua_getfield(L, -1, name.c_str());
        if (lua_type(L, -1) == LUA_TSTRING)
        {
            lua_pushliteral(L, ", ");
            lua_pushlstring(L, header.value.data(), header.value.size());
            lua_concat(L, 3);
        }
        else
        {
            lua_pop(L, 1);
            lua_pushlstring(L, header.value.data(), header.value.size());
        }
        lua_setfield(L, -2, name.c_str());
    }
}

int pushHeader(lua_State* L,
               const scriptnet::HttpHeaders& headers,
               const char* wanted,
               size_t length)
{
    bool found = false;
    std::string value;
    std::string name = scriptnet::lowerAscii(std::string(wanted, length));
    for (const scriptnet::HttpHeader& header : headers)
    {
        if (scriptnet::lowerAscii(header.name) == name)
        {
            if (found)
            {
                value += ", ";
            }
            value += header.value;
            found = true;
        }
    }
    if (found)
    {
        lua_pushlstring(L, value.data(), value.size());
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

// Body reads return promises so a later streamed body needs no API change.
int pushResolved(lua_State* L)
{
    int valueIdx = lua_gettop(L);
    lua_newrive<ScriptedPromise>(L, lua_mainthread(L))->resolve(L, valueIdx);
    return 1;
}

int responseIndex(lua_State* L)
{
    auto* self = lua_torive<ScriptedHttpResponse>(L, 1);
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (key == nullptr)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    const scriptnet::HttpResponse& response = self->response;
    switch (atom)
    {
        case (int)LuaAtoms::status:
            lua_pushinteger(L, response.status);
            return 1;
        case (int)LuaAtoms::statusText:
            lua_pushlstring(L,
                            response.statusText.data(),
                            response.statusText.size());
            return 1;
        case (int)LuaAtoms::ok:
            lua_pushboolean(L, response.status >= 200 && response.status < 300);
            return 1;
        case (int)LuaAtoms::url:
            lua_pushlstring(L, response.url.data(), response.url.size());
            return 1;
        case (int)LuaAtoms::headers:
            pushHeaders(L, response.headers);
            return 1;
        default:
            break;
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedHttpResponse::luaName);
    return 0;
}

int responseNamecall(lua_State* L)
{
    int atom;
    const char* name = lua_namecallatom(L, &atom);
    auto* self = lua_torive<ScriptedHttpResponse>(L, 1);
    if (name != nullptr)
    {
        const std::vector<uint8_t>& body = self->response.body;
        switch (atom)
        {
            case (int)LuaAtoms::text:
                lua_pushlstring(L,
                                body.empty() ? "" : (const char*)body.data(),
                                body.size());
                return pushResolved(L);
            case (int)LuaAtoms::arrayBuffer:
            {
                void* data = lua_newbuffer(L, body.size());
                if (!body.empty())
                {
                    memcpy(data, body.data(), body.size());
                }
                return pushResolved(L);
            }
            case (int)LuaAtoms::header:
            {
                size_t length = 0;
                const char* wanted = luaL_checklstring(L, 2, &length);
                return pushHeader(L, self->response.headers, wanted, length);
            }
            default:
                break;
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               name != nullptr ? name : "?",
               ScriptedHttpResponse::luaName);
    return 0;
}
} // namespace

int luaopen_rive_scriptnet(lua_State* L)
{
    lua_register_rive<ScriptedHttpResponse>(L);
    lua_pushcfunction(L, responseIndex, nullptr);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, responseNamecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_pushcfunction(L, scriptnetFetch, "fetch");
    lua_setglobal(L, "fetch");
    return 0;
}

#endif // WITH_RIVE_SCRIPTING && WITH_RIVE_SCRIPTNET
