#ifdef WITH_RIVE_SCRIPTING

#include "rive/lua/rive_lua_libs.hpp"
#include "lua.h"
#include "lualib.h"

#include <cassert>
#include <cstring>
#include <vector>
#include <unordered_map>

// ============================================================================
// Native path — uses Bitmap::decode via WorkPool
// ============================================================================

#ifndef __EMSCRIPTEN__

#include "rive/async/work_pool.hpp"
#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace rive
{

#ifdef RIVE_DECODERS
class ImageDecodeTask : public WorkTask
{
public:
    std::vector<uint8_t> m_encodedData;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    lua_State* m_state = nullptr;
    int m_promiseRef = LUA_NOREF;
    std::unique_ptr<Bitmap> m_bitmap;

    bool execute() override
    {
        m_bitmap = Bitmap::decode(m_encodedData.data(), m_encodedData.size());
        if (!m_bitmap)
        {
            m_errorMessage = "failed to decode image data";
            return false;
        }
        if (m_bitmap->pixelFormat() != Bitmap::PixelFormat::RGBAPremul)
        {
            m_bitmap->pixelFormat(Bitmap::PixelFormat::RGBAPremul);
        }
        m_width = m_bitmap->width();
        m_height = m_bitmap->height();
        return true;
    }

    void onComplete() override
    {
        if (!m_state || m_promiseRef == LUA_NOREF)
            return;
        lua_State* L = m_state;
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_promiseRef);
        auto* promise = lua_torive<ScriptedPromise>(L, -1, true);
        lua_pop(L, 1);
        if (promise && promise->isPending())
        {
            if (m_bitmap->pixelFormat() != Bitmap::PixelFormat::RGBAPremul)
            {
                lua_pushstring(L,
                               "internal error: decoded image is not "
                               "RGBAPremul");
                promise->reject(L, lua_gettop(L));
                lua_pop(L, 1);
                m_bitmap.reset();
                lua_unref(L, m_promiseRef);
                m_promiseRef = LUA_NOREF;
                return;
            }
            lua_newtable(L);
            size_t pixelSize = (size_t)m_width * m_height * 4;
            void* buf = lua_newbuffer(L, pixelSize);
            memcpy(buf, m_bitmap->bytes(), pixelSize);
            lua_setfield(L, -2, "data");
            lua_pushnumber(L, m_width);
            lua_setfield(L, -2, "width");
            lua_pushnumber(L, m_height);
            lua_setfield(L, -2, "height");
            promise->resolve(L, lua_gettop(L));
            lua_pop(L, 1);
        }
        // Release decoded bitmap immediately — the pixels have been
        // copied to the Lua buffer. The TaskRef userdata in the onCancel
        // closure may keep this task alive until GC, so eagerly free the
        // large allocation.
        m_bitmap.reset();
        m_encodedData.clear();
        m_encodedData.shrink_to_fit();
        lua_unref(L, m_promiseRef);
        m_promiseRef = LUA_NOREF;
    }

    void onError(const std::string& error) override
    {
        if (!m_state || m_promiseRef == LUA_NOREF)
            return;
        lua_State* L = m_state;
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_promiseRef);
        auto* promise = lua_torive<ScriptedPromise>(L, -1, true);
        lua_pop(L, 1);
        if (promise && promise->isPending())
        {
            lua_pushstring(L, error.c_str());
            promise->reject(L, lua_gettop(L));
            lua_pop(L, 1);
        }
        lua_unref(L, m_promiseRef);
        m_promiseRef = LUA_NOREF;
    }

    void onCancel() override
    {
        m_promiseRef = LUA_NOREF;
        m_state = nullptr;
    }
};
#endif // RIVE_DECODERS

} // namespace rive

int context_decodeImage_impl(lua_State* L)
{
    using namespace rive;

    size_t len = 0;
    const void* data = nullptr;
    if (lua_isbuffer(L, 2))
        data = lua_tobuffer(L, 2, &len);
    else
    {
        luaL_typeerror(L, 2, "buffer");
        return 0;
    }
    if (!data || len == 0)
    {
        luaL_error(L, "decodeImage: empty buffer");
        return 0;
    }

#ifndef RIVE_DECODERS
    luaL_error(L, "decodeImage: not supported on this platform");
    return 0;
#else
    auto* ctx = static_cast<ScriptingContext*>(lua_getthreaddata(L));
    WorkPool* wp = ctx->workPool();

    lua_State* mainThread = lua_mainthread(L);
    lua_newrive<ScriptedPromise>(L, mainThread);
    int promiseIdx = lua_gettop(L);
    lua_pushvalue(L, promiseIdx);
    int promiseRef = lua_ref(L, -1);
    lua_pop(L, 1);

    auto task = make_rcp<ImageDecodeTask>();
    task->m_encodedData.assign(static_cast<const uint8_t*>(data),
                               static_cast<const uint8_t*>(data) + len);
    task->m_state = mainThread;
    task->m_promiseRef = promiseRef;
    task->setOwnerId(ctx->ownerId());

    // Wrap the rcp<WorkTask> in a Lua userdata with a destructor so the
    // task stays alive as long as the onCancel closure exists. This
    // prevents a use-after-free if the WorkPool is destroyed (dropping
    // its rcp) before the promise is cancelled.
    struct TaskRef
    {
        rcp<WorkTask> ref;
    };
    auto* taskUD = static_cast<TaskRef*>(
        lua_newuserdatadtor(L, sizeof(TaskRef), [](void* p) {
            static_cast<TaskRef*>(p)->~TaskRef();
        }));
    new (taskUD) TaskRef{task}; // copy rcp before submit moves it
    int taskUDIdx = lua_gettop(L);

    wp->submit(std::move(task));

    // Register onCancel hook: cancel the WorkPool task so the decode
    // thread skips execution (or the result is discarded), and release
    // the promise registry reference so the Promise can be collected.
    auto* promise = lua_torive<ScriptedPromise>(L, promiseIdx);
    lua_pushvalue(L, taskUDIdx);
    lua_remove(L, taskUDIdx); // remove original, closure upvalue has it
    lua_pushinteger(L, promiseRef);
    lua_pushcclosurek(
        L,
        [](lua_State* L) -> int {
            auto* tr =
                static_cast<TaskRef*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (tr && tr->ref)
                tr->ref->cancel();
            int pRef = (int)lua_tointeger(L, lua_upvalueindex(2));
            lua_unref(L, pRef);
            return 0;
        },
        nullptr,
        2,
        nullptr);
    promise->m_onCancelRef = lua_ref(L, -1);
    lua_pop(L, 1);

    lua_pushvalue(L, promiseIdx);
    return 1;
#endif // RIVE_DECODERS
}

// ============================================================================
// WASM path — uses browser's createImageBitmap, no WorkPool
// ============================================================================

#else // __EMSCRIPTEN__

#include "rive/async/browser_image_decode.hpp"

namespace
{

// Track in-flight decodes so the callback can find the Lua state + promise.
struct PendingDecode
{
    lua_State* state;
    int promiseRef;
};
std::unordered_map<uint32_t, PendingDecode> s_pendingDecodes;

void settleDecode(uint32_t id,
                  uint32_t width,
                  uint32_t height,
                  rive::Span<const uint8_t> pixels,
                  const char* error)
{
    using namespace rive;

    auto it = s_pendingDecodes.find(id);
    if (it == s_pendingDecodes.end())
    {
        return;
    }
    auto [L, promiseRef] = it->second;
    s_pendingDecodes.erase(it);

    lua_rawgeti(L, LUA_REGISTRYINDEX, promiseRef);
    auto* promise = lua_torive<ScriptedPromise>(L, -1, true);
    lua_pop(L, 1);

    if (promise && promise->isPending())
    {
        if (error != nullptr)
        {
            lua_pushstring(L, error);
            promise->reject(L, lua_gettop(L));
        }
        else
        {
            lua_newtable(L);

            void* buf = lua_newbuffer(L, pixels.size());
            memcpy(buf, pixels.data(), pixels.size());
            lua_setfield(L, -2, "data");

            lua_pushnumber(L, width);
            lua_setfield(L, -2, "width");

            lua_pushnumber(L, height);
            lua_setfield(L, -2, "height");

            promise->resolve(L, lua_gettop(L));
        }
        lua_pop(L, 1);
    }
    lua_unref(L, promiseRef);
}

} // namespace

// Cancel all pending decodes for a Lua state (called on shutdown).
// Must be called while mainThread is still valid so we can unref promises.
namespace rive
{
void wasm_cancelPendingDecodes(lua_State* mainThread)
{
    for (auto it = s_pendingDecodes.begin(); it != s_pendingDecodes.end();)
    {
        if (it->second.state == mainThread)
        {
            cancelBrowserImageDecode(it->first);
            lua_unref(mainThread, it->second.promiseRef);
            it = s_pendingDecodes.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// Whether a decode mainThread started is still waiting on the browser. These
// bypass the WorkPool, so its owner-scoped check can't see them.
bool wasm_hasPendingDecodes(lua_State* mainThread)
{
    for (const auto& entry : s_pendingDecodes)
    {
        if (entry.second.state == mainThread)
        {
            return true;
        }
    }
    return false;
}
} // namespace rive

int context_decodeImage_impl(lua_State* L)
{
    using namespace rive;

    size_t len = 0;
    const void* data = nullptr;
    if (lua_isbuffer(L, 2))
        data = lua_tobuffer(L, 2, &len);
    else
    {
        luaL_typeerror(L, 2, "buffer");
        return 0;
    }
    if (!data || len == 0)
    {
        luaL_error(L, "decodeImage: empty buffer");
        return 0;
    }

    lua_State* mainThread = lua_mainthread(L);
    lua_newrive<ScriptedPromise>(L, mainThread);
    int promiseIdx = lua_gettop(L);
    lua_pushvalue(L, promiseIdx);
    int promiseRef = lua_ref(L, -1);
    lua_pop(L, 1);

    // The decode settles on a later event loop turn, after this registers.
    uint32_t id = startBrowserImageDecode(static_cast<const uint8_t*>(data),
                                          static_cast<uint32_t>(len),
                                          settleDecode);
    s_pendingDecodes[id] = {mainThread, promiseRef};

    // Register onCancel hook: erase from pending decodes so the JS
    // callback becomes a no-op (the browser will still finish decoding,
    // but the result is discarded).
    auto* promise = lua_torive<ScriptedPromise>(L, promiseIdx);
    lua_pushinteger(L, id);
    lua_pushinteger(L, promiseRef);
    lua_pushcclosurek(
        L,
        [](lua_State* L) -> int {
            uint32_t decodeId = (uint32_t)lua_tointeger(L, lua_upvalueindex(1));
            int pRef = (int)lua_tointeger(L, lua_upvalueindex(2));
            s_pendingDecodes.erase(decodeId);
            cancelBrowserImageDecode(decodeId);
            lua_unref(L, pRef);
            return 0;
        },
        nullptr,
        2,
        nullptr);
    promise->m_onCancelRef = lua_ref(L, -1);
    lua_pop(L, 1);

    lua_pushvalue(L, promiseIdx);
    return 1;
}

#endif // __EMSCRIPTEN__

#endif // WITH_RIVE_SCRIPTING
