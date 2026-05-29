#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"

#include "lua.h"
#include "lualib.h"

#include <cstring>

namespace rive
{

// ---------------------------------------------------------------------------
// ScriptedPromise – a lightweight Promise/A+ for Luau with cancellation
// ---------------------------------------------------------------------------
//
// States: Pending → Fulfilled | Rejected | Cancelled  (one-way, immutable)
//
// Cancellation follows the Roblox model:
//   - cancel() propagates DOWN to consumers (chained promises)
//   - cancel() propagates UP to parent when ALL consumers are cancelled
//   - An onCancel hook fires to clean up in-flight work (e.g. image decode)
//
// Parent/consumer refs form a tree. To break the cycle (parent ↔ consumer),
// all refs are released eagerly on any terminal transition via
// promise_cleanupLinks().

// ── helpers ────────────────────────────────────────────────────────────────

static void promise_notifyCallbacks(lua_State* L, ScriptedPromise* p);
static void handleCoroutineCompletion(lua_State* L,
                                      lua_State* co,
                                      int status,
                                      int coIdx,
                                      int asyncPromiseIdx);

// Release parent/consumer/onCancel refs after a terminal transition.
static void promise_cleanupLinks(lua_State* L, ScriptedPromise* p)
{
    if (p->m_parentRef != LUA_NOREF)
    {
        lua_unref(L, p->m_parentRef);
        p->m_parentRef = LUA_NOREF;
    }
    for (int ref : p->m_consumerRefs)
    {
        if (ref != LUA_NOREF)
            lua_unref(L, ref);
    }
    p->m_consumerRefs.clear();
    if (p->m_onCancelRef != LUA_NOREF)
    {
        lua_unref(L, p->m_onCancelRef);
        p->m_onCancelRef = LUA_NOREF;
    }
}

// ── resolve / reject / cancel ─────────────────────────────────────────────

void ScriptedPromise::resolve(lua_State* L, int valueIdx)
{
    if (m_promiseState != PromiseState::Pending)
        return;

    // Promise/A+ flattening: if the resolved value is itself a Promise,
    // adopt its state instead of resolving with the Promise object.
    auto* inner = lua_torive<ScriptedPromise>(L, valueIdx, true);
    if (inner && inner != this)
    {
        if (inner->isFulfilled())
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, inner->resultRef());
            resolve(L, lua_gettop(L));
            lua_pop(L, 1);
            return;
        }
        if (inner->isRejected())
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, inner->resultRef());
            reject(L, lua_gettop(L));
            lua_pop(L, 1);
            return;
        }
        if (inner->isCancelled())
        {
            cancel(L);
            return;
        }
        // Still pending — adopt by chaining. Handled by the overload
        // that has a selfRef. Direct resolve() callers that don't have
        // a ref to self can't adopt pending promises; assert that this
        // path is unreachable in practice.
        assert(false && "resolve() with pending inner promise requires "
                        "selfRef — use the overload");
        return;
    }

    m_promiseState = PromiseState::Fulfilled;
    lua_pushvalue(L, valueIdx);
    m_resultRef = lua_ref(L, -1);
    lua_pop(L, 1);
    promise_notifyCallbacks(L, this);
    promise_cleanupLinks(L, this);
}

void ScriptedPromise::resolve(lua_State* L, int valueIdx, int selfRef)
{
    if (m_promiseState != PromiseState::Pending)
        return;

    // Promise/A+ flattening with self registry ref for pending adoption.
    auto* inner = lua_torive<ScriptedPromise>(L, valueIdx, true);
    if (inner && inner != this)
    {
        if (inner->isFulfilled())
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, inner->resultRef());
            resolve(L, lua_gettop(L));
            lua_pop(L, 1);
            return;
        }
        if (inner->isRejected())
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, inner->resultRef());
            reject(L, lua_gettop(L));
            lua_pop(L, 1);
            return;
        }
        if (inner->isCancelled())
        {
            cancel(L);
            return;
        }
        // Still pending — wire this as a consumer of inner so that when
        // inner settles, it forwards the result to this promise via the
        // no-handler ThenCallback propagation path.
        // Release any existing parent ref (this promise may already be
        // chained from another promise via andThen).
        if (m_parentRef != LUA_NOREF)
            lua_unref(L, m_parentRef);
        lua_pushvalue(L, valueIdx); // push inner for ref
        m_parentRef = lua_ref(L, -1);
        lua_pop(L, 1);

        // selfRef is a registry ref to this promise's userdata — duplicate
        // it for the consumer list and the chained callback.
        lua_rawgeti(L, LUA_REGISTRYINDEX, selfRef);
        int consumerRef = lua_ref(L, -1); // for m_consumerRefs
        lua_pop(L, 1);

        lua_rawgeti(L, LUA_REGISTRYINDEX, selfRef);
        int chainedRef = lua_ref(L, -1); // for ThenCallback
        lua_pop(L, 1);

        inner->m_consumerRefs.push_back(consumerRef);
        {
            ScriptedPromise::ThenCallback cb;
            cb.chainedPromiseRef = chainedRef;
            inner->m_thenCallbacks.push_back(cb);
        }
        return;
    }

    m_promiseState = PromiseState::Fulfilled;
    lua_pushvalue(L, valueIdx);
    m_resultRef = lua_ref(L, -1);
    lua_pop(L, 1);
    promise_notifyCallbacks(L, this);
    promise_cleanupLinks(L, this);
}

void ScriptedPromise::reject(lua_State* L, int errorIdx)
{
    if (m_promiseState != PromiseState::Pending)
        return;
    m_promiseState = PromiseState::Rejected;
    lua_pushvalue(L, errorIdx);
    m_resultRef = lua_ref(L, -1);
    lua_pop(L, 1);
    promise_notifyCallbacks(L, this);
    promise_cleanupLinks(L, this);
}

void ScriptedPromise::cancel(lua_State* L)
{
    if (m_promiseState != PromiseState::Pending)
        return;

    m_promiseState = PromiseState::Cancelled;

    // 1. Fire the onCancel hook (if registered).
    if (m_onCancelRef != LUA_NOREF)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_onCancelRef);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        {
            lua_pop(L, 1); // pop error object
        }
    }

    // 2. Propagate cancel DOWN to all consumers.
    for (int ref : m_consumerRefs)
    {
        if (ref != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            auto* consumer = lua_torive<ScriptedPromise>(L, -1, true);
            lua_pop(L, 1);
            if (consumer && consumer->isPending())
                consumer->cancel(L);
        }
    }

    // 3. Notify parent: cancel parent if ALL its consumers are now cancelled.
    if (m_parentRef != LUA_NOREF)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_parentRef);
        auto* parent = lua_torive<ScriptedPromise>(L, -1, true);
        lua_pop(L, 1);
        if (parent && parent->isPending())
        {
            bool allCancelled = true;
            for (int cRef : parent->m_consumerRefs)
            {
                if (cRef != LUA_NOREF)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, cRef);
                    auto* c = lua_torive<ScriptedPromise>(L, -1, true);
                    lua_pop(L, 1);
                    if (c && !c->isCancelled())
                    {
                        allCancelled = false;
                        break;
                    }
                }
            }
            if (allCancelled)
                parent->cancel(L);
        }
    }

    // 4. Clean up callback refs (they will never fire).
    for (auto& cb : m_thenCallbacks)
    {
        // Fire cancelRef if present (async/await cleanup).
        if (cb.cancelRef != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, cb.cancelRef);
            if (lua_pcall(L, 0, 0, 0) != LUA_OK)
            {
                lua_pop(L, 1);
            }
            lua_unref(L, cb.cancelRef);
        }
        if (cb.successRef != LUA_NOREF)
            lua_unref(L, cb.successRef);
        if (cb.failureRef != LUA_NOREF)
            lua_unref(L, cb.failureRef);
        if (cb.chainedPromiseRef != LUA_NOREF)
            lua_unref(L, cb.chainedPromiseRef);
    }
    m_thenCallbacks.clear();
    for (auto& cb : m_finallyCallbacks)
    {
        if (cb.callbackRef != LUA_NOREF)
            lua_unref(L, cb.callbackRef);
        if (cb.chainedPromiseRef != LUA_NOREF)
            lua_unref(L, cb.chainedPromiseRef);
    }
    m_finallyCallbacks.clear();

    // 5. Clean up parent/consumer/onCancel refs.
    promise_cleanupLinks(L, this);
}

ScriptedPromise::~ScriptedPromise()
{
    // Release any held Lua refs.  m_state may be the main thread of a
    // still-alive VM, so only unref if we actually have refs.
    if (m_resultRef != LUA_NOREF && m_state)
        lua_unref(m_state, m_resultRef);
    for (auto& cb : m_thenCallbacks)
    {
        if (cb.successRef != LUA_NOREF && m_state)
            lua_unref(m_state, cb.successRef);
        if (cb.failureRef != LUA_NOREF && m_state)
            lua_unref(m_state, cb.failureRef);
        if (cb.chainedPromiseRef != LUA_NOREF && m_state)
            lua_unref(m_state, cb.chainedPromiseRef);
        if (cb.cancelRef != LUA_NOREF && m_state)
            lua_unref(m_state, cb.cancelRef);
    }
    for (auto& cb : m_finallyCallbacks)
    {
        if (cb.callbackRef != LUA_NOREF && m_state)
            lua_unref(m_state, cb.callbackRef);
        if (cb.chainedPromiseRef != LUA_NOREF && m_state)
            lua_unref(m_state, cb.chainedPromiseRef);
    }
    if (m_parentRef != LUA_NOREF && m_state)
        lua_unref(m_state, m_parentRef);
    for (int ref : m_consumerRefs)
    {
        if (ref != LUA_NOREF && m_state)
            lua_unref(m_state, ref);
    }
    if (m_onCancelRef != LUA_NOREF && m_state)
        lua_unref(m_state, m_onCancelRef);
}

// ── callback dispatch ──────────────────────────────────────────────────────

static void promise_notifyCallbacks(lua_State* L, ScriptedPromise* p)
{
    // Cancelled promises don't fire andThen/catch/finally handlers.
    if (p->isCancelled())
        return;

    bool fulfilled = p->isFulfilled();

    // Push the result/error value once.
    lua_rawgeti(L, LUA_REGISTRYINDEX, p->resultRef());

    int resultIdx = lua_gettop(L);

    // ── andThen / catch callbacks ──
    for (auto& cb : p->m_thenCallbacks)
    {
        // Resolve the chained promise ref to a pointer.
        ScriptedPromise* chained = nullptr;
        if (cb.chainedPromiseRef != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, cb.chainedPromiseRef);
            chained = lua_torive<ScriptedPromise>(L, -1, true);
            lua_pop(L, 1);
            // Skip cancelled consumers.
            if (chained && chained->isCancelled())
                chained = nullptr;
        }

        int handlerRef = fulfilled ? cb.successRef : cb.failureRef;
        if (handlerRef != LUA_NOREF)
        {
            // Call the handler with the result.
            lua_rawgeti(L, LUA_REGISTRYINDEX, handlerRef);
            lua_pushvalue(L, resultIdx);
            int status = lua_pcall(L, 1, 1, 0);
            if (status == LUA_OK)
            {
                // Handler returned a value — resolve chained promise with it.
                // Use the selfRef overload for Promise/A+ flattening.
                if (chained)
                    chained->resolve(L, lua_gettop(L), cb.chainedPromiseRef);
                lua_pop(L, 1);
            }
            else
            {
                // Handler errored — reject chained promise.
                if (chained)
                    chained->reject(L, lua_gettop(L));
                lua_pop(L, 1); // pop error
            }
        }
        else
        {
            // No handler for this branch — propagate to chained promise.
            // No flattening needed here — the value is already unwrapped
            // from the parent, not a handler return value.
            if (chained)
            {
                if (fulfilled)
                    chained->resolve(L, resultIdx, cb.chainedPromiseRef);
                else
                    chained->reject(L, resultIdx);
            }
        }

        // Unref handlers.
        if (cb.successRef != LUA_NOREF)
            lua_unref(L, cb.successRef);
        if (cb.failureRef != LUA_NOREF)
            lua_unref(L, cb.failureRef);
        if (cb.chainedPromiseRef != LUA_NOREF)
            lua_unref(L, cb.chainedPromiseRef);
        if (cb.cancelRef != LUA_NOREF)
            lua_unref(L, cb.cancelRef);
    }

    // ── finally callbacks ──
    for (auto& cb : p->m_finallyCallbacks)
    {
        ScriptedPromise* chained = nullptr;
        if (cb.chainedPromiseRef != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, cb.chainedPromiseRef);
            chained = lua_torive<ScriptedPromise>(L, -1, true);
            lua_pop(L, 1);
            if (chained && chained->isCancelled())
                chained = nullptr;
        }

        if (cb.callbackRef != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, cb.callbackRef);
            int status = lua_pcall(L, 0, 0, 0);
            if (status != LUA_OK)
            {
                // finally threw — reject the chained promise with the error
                // instead of propagating the original result.
                if (chained)
                    chained->reject(L, lua_gettop(L));
                lua_pop(L, 1); // pop error from stack
                lua_unref(L, cb.callbackRef);
                if (cb.chainedPromiseRef != LUA_NOREF)
                    lua_unref(L, cb.chainedPromiseRef);
                continue;
            }
            lua_unref(L, cb.callbackRef);
        }
        // Propagate original result to chained promise.
        if (chained)
        {
            if (fulfilled)
                chained->resolve(L, resultIdx);
            else
                chained->reject(L, resultIdx);
        }
        if (cb.chainedPromiseRef != LUA_NOREF)
            lua_unref(L, cb.chainedPromiseRef);
    }

    lua_pop(L, 1); // pop result value

    p->m_thenCallbacks.clear();
    p->m_finallyCallbacks.clear();
}

// ── helper: wire parent/consumer links for a chained promise ────────────

// Call after creating a chained promise. `source` is at stack index
// `sourceIdx`, chained promise is at the top of the stack.
static void promise_wireLinks(lua_State* L,
                              ScriptedPromise* source,
                              int sourceIdx,
                              ScriptedPromise* chained)
{
    // Consumer ref: source → chained (for downward cancel propagation).
    lua_pushvalue(L, -1); // push chained
    int consumerRef = lua_ref(L, -1);
    lua_pop(L, 1);
    source->m_consumerRefs.push_back(consumerRef);

    // Parent ref: chained → source (for upward cancel propagation).
    lua_pushvalue(L, sourceIdx);
    chained->m_parentRef = lua_ref(L, -1);
    lua_pop(L, 1);
}

// ── namecall: p:andThen(), p:catch(), p:finally(), p:cancel(), etc. ─────

static int promise_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    auto* p = lua_torive<ScriptedPromise>(L, 1);

    if (atom == (int)LuaAtoms::andThen)
    {
        // andThen(onFulfilled [, onRejected]) -> Promise
        int successRef = LUA_NOREF;
        int failureRef = LUA_NOREF;
        if (lua_isfunction(L, 2))
        {
            lua_pushvalue(L, 2);
            successRef = lua_ref(L, -1);
            lua_pop(L, 1);
        }
        if (lua_isfunction(L, 3))
        {
            lua_pushvalue(L, 3);
            failureRef = lua_ref(L, -1);
            lua_pop(L, 1);
        }

        auto* chained = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
        // The chained promise is now at the top of the stack.
        // Create a registry ref to prevent GC while the parent is pending.
        lua_pushvalue(L, -1);
        int chainedRef = lua_ref(L, -1);
        lua_pop(L, 1);

        // Wire parent/consumer links.
        promise_wireLinks(L, p, 1, chained);

        {
            ScriptedPromise::ThenCallback cb;
            cb.successRef = successRef;
            cb.failureRef = failureRef;
            cb.chainedPromiseRef = chainedRef;
            p->m_thenCallbacks.push_back(cb);
        }

        if (!p->isPending())
            promise_notifyCallbacks(L, p);

        return 1; // return chained promise (still on stack)
    }
    else if (atom == (int)LuaAtoms::catch_)
    {
        // catch(onRejected) -> Promise  (sugar for andThen(nil, onRejected))
        int failureRef = LUA_NOREF;
        if (lua_isfunction(L, 2))
        {
            lua_pushvalue(L, 2);
            failureRef = lua_ref(L, -1);
            lua_pop(L, 1);
        }

        auto* chained = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
        lua_pushvalue(L, -1);
        int chainedRef = lua_ref(L, -1);
        lua_pop(L, 1);

        promise_wireLinks(L, p, 1, chained);

        {
            ScriptedPromise::ThenCallback cb;
            cb.failureRef = failureRef;
            cb.chainedPromiseRef = chainedRef;
            p->m_thenCallbacks.push_back(cb);
        }

        if (!p->isPending())
            promise_notifyCallbacks(L, p);

        return 1;
    }
    else if (atom == (int)LuaAtoms::finally_)
    {
        int cbRef = LUA_NOREF;
        if (lua_isfunction(L, 2))
        {
            lua_pushvalue(L, 2);
            cbRef = lua_ref(L, -1);
            lua_pop(L, 1);
        }

        auto* chained = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
        lua_pushvalue(L, -1);
        int chainedRef = lua_ref(L, -1);
        lua_pop(L, 1);

        promise_wireLinks(L, p, 1, chained);

        {
            ScriptedPromise::FinallyCallback cb;
            cb.callbackRef = cbRef;
            cb.chainedPromiseRef = chainedRef;
            p->m_finallyCallbacks.push_back(cb);
        }

        if (!p->isPending())
            promise_notifyCallbacks(L, p);

        return 1;
    }
    else if (atom == (int)LuaAtoms::cancel)
    {
        p->cancel(L);
        lua_pushvalue(L, 1); // return self for chaining
        return 1;
    }
    else if (atom == (int)LuaAtoms::onCancel)
    {
        // onCancel(fn) — register a cancellation hook.
        if (lua_isfunction(L, 2))
        {
            if (p->m_onCancelRef != LUA_NOREF)
                lua_unref(L, p->m_onCancelRef);
            lua_pushvalue(L, 2);
            p->m_onCancelRef = lua_ref(L, -1);
            lua_pop(L, 1);
        }
        lua_pushvalue(L, 1); // return self
        return 1;
    }
    else if (atom == (int)LuaAtoms::getStatus)
    {
        switch (p->m_promiseState)
        {
            case PromiseState::Pending:
                lua_pushstring(L, "Pending");
                break;
            case PromiseState::Fulfilled:
                lua_pushstring(L, "Fulfilled");
                break;
            case PromiseState::Rejected:
                lua_pushstring(L, "Rejected");
                break;
            case PromiseState::Cancelled:
                lua_pushstring(L, "Cancelled");
                break;
        }
        return 1;
    }

    luaL_error(L, "'%s' is not a valid method of Promise", str);
    return 0;
}

// ── static methods: Promise.resolve(), Promise.reject(), Promise.all() ───

static int promise_static_resolve(lua_State* L)
{
    auto* p = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
    int pIdx = lua_gettop(L); // p is on top of stack
    if (pIdx >= 2)
    {
        // Promise.resolve(value) — use selfRef overload for A+ flattening.
        lua_pushvalue(L, pIdx);
        int selfRef = lua_ref(L, -1);
        lua_pop(L, 1);
        p->resolve(L, 1, selfRef); // arg 1 is the value (before we pushed p)
        lua_unref(L, selfRef);
    }
    else
    {
        // Promise.resolve() with no value → resolve with nil
        lua_pushnil(L);
        p->resolve(L, lua_gettop(L));
        lua_pop(L, 1);
    }
    return 1;
}

static int promise_static_reject(lua_State* L)
{
    auto* p = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
    if (lua_gettop(L) >= 2)
    {
        p->reject(L, 1);
    }
    else
    {
        lua_pushstring(L, "rejected");
        p->reject(L, lua_gettop(L));
        lua_pop(L, 1);
    }
    return 1;
}

// ── Promise.new(executor) ───────────────────────────────────────────────

static int promise_static_new(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    auto* p = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
    int promiseIdx = lua_gettop(L);

    // Create resolve callback — captures promise as direct upvalue (no
    // registry ref needed; GC keeps it alive via the closure).
    lua_pushvalue(L, promiseIdx);
    lua_pushcclosurek(
        L,
        [](lua_State* L) -> int {
            auto* promise =
                lua_torive<ScriptedPromise>(L, lua_upvalueindex(1), true);
            if (promise && promise->isPending())
            {
                if (lua_gettop(L) >= 1)
                {
                    // Use selfRef overload for A+ flattening.
                    lua_pushvalue(L, lua_upvalueindex(1));
                    int selfRef = lua_ref(L, -1);
                    lua_pop(L, 1);
                    promise->resolve(L, 1, selfRef);
                    lua_unref(L, selfRef);
                }
                else
                {
                    lua_pushnil(L);
                    promise->resolve(L, lua_gettop(L));
                    lua_pop(L, 1);
                }
            }
            return 0;
        },
        nullptr,
        1,
        nullptr);
    int resolveIdx = lua_gettop(L);

    // Create reject callback.
    lua_pushvalue(L, promiseIdx);
    lua_pushcclosurek(
        L,
        [](lua_State* L) -> int {
            auto* promise =
                lua_torive<ScriptedPromise>(L, lua_upvalueindex(1), true);
            if (promise && promise->isPending())
            {
                if (lua_gettop(L) >= 1)
                    promise->reject(L, 1);
                else
                {
                    lua_pushstring(L, "rejected");
                    promise->reject(L, lua_gettop(L));
                    lua_pop(L, 1);
                }
            }
            return 0;
        },
        nullptr,
        1,
        nullptr);
    int rejectIdx = lua_gettop(L);

    // Create onCancel callback.
    lua_pushvalue(L, promiseIdx);
    lua_pushcclosurek(
        L,
        [](lua_State* L) -> int {
            auto* promise =
                lua_torive<ScriptedPromise>(L, lua_upvalueindex(1), true);
            if (promise && lua_isfunction(L, 1))
            {
                if (promise->m_onCancelRef != LUA_NOREF)
                    lua_unref(L, promise->m_onCancelRef);
                lua_pushvalue(L, 1);
                promise->m_onCancelRef = lua_ref(L, -1);
                lua_pop(L, 1);
            }
            return 0;
        },
        nullptr,
        1,
        nullptr);
    int onCancelIdx = lua_gettop(L);

    // Call executor(resolve, reject, onCancel).
    lua_pushvalue(L, 1); // push executor
    lua_pushvalue(L, resolveIdx);
    lua_pushvalue(L, rejectIdx);
    lua_pushvalue(L, onCancelIdx);
    int status = lua_pcall(L, 3, 0, 0);
    if (status != LUA_OK)
    {
        // Executor threw — reject the promise with the error.
        if (p->isPending())
            p->reject(L, lua_gettop(L));
        lua_pop(L, 1); // pop error
    }

    // Clean up intermediates and return the promise.
    lua_pushvalue(L, promiseIdx);
    return 1;
}

// ── Promise.all ─────────────────────────────────────────────────────────

static int promise_static_all(lua_State* L)
{
    // Promise.all({p1, p2, ...}) -> Promise<{v1, v2, ...}>
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = lua_objlen(L, 1);

    auto* result = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
    int resultIdx = lua_gettop(L);

    if (n == 0)
    {
        // Empty array → resolve immediately with empty table.
        lua_newtable(L);
        result->resolve(L, lua_gettop(L));
        lua_pop(L, 1);
        lua_pushvalue(L, resultIdx);
        return 1;
    }

    // Shared state table — stays on L's stack, captured as direct upvalue
    // by all success/failure closures. No registry ref needed.
    // Fields: { remaining=n, results={}, done=false }
    lua_newtable(L);
    int stateIdx = lua_gettop(L);
    lua_pushinteger(L, n);
    lua_setfield(L, stateIdx, "remaining");
    lua_newtable(L);
    lua_setfield(L, stateIdx, "results");
    lua_pushboolean(L, false);
    lua_setfield(L, stateIdx, "done");

    // Chained refs table — stays on stack, captured as direct upvalue.
    // Populated during the loop below with registry refs to chained
    // promises for cancellation cleanup.
    lua_newtable(L);
    int chainedRefsTableIdx = lua_gettop(L);

    for (int i = 1; i <= n; i++)
    {
        lua_rawgeti(L, 1, i); // push promises[i]
        auto* pi = lua_torive<ScriptedPromise>(L, -1, true);
        int piIdx = lua_gettop(L);
        if (!pi)
        {
            lua_pop(L, 1);
            luaL_error(L, "Promise.all: element %d is not a Promise", i);
            return 0;
        }

        // Create chained promise for this input.
        auto* chained = lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
        int chainedIdx = lua_gettop(L);

        // Wire parent/consumer links: pi → chained.
        promise_wireLinks(L, pi, piIdx, chained);

        // Registry ref for ThenCallback dispatch.
        lua_pushvalue(L, chainedIdx);
        int chainedRef = lua_ref(L, -1);
        lua_pop(L, 1);

        // Store a separate ref in the chainedRefs table for cancellation.
        lua_pushvalue(L, chainedIdx);
        int cancelCollectionRef = lua_ref(L, -1);
        lua_pop(L, 1);
        lua_pushinteger(L, cancelCollectionRef);
        lua_rawseti(L, chainedRefsTableIdx, i);

        // Success closure: upvalues = (index, state, resultPromise,
        // chainedRefs)
        lua_pushinteger(L, i);
        lua_pushvalue(L, stateIdx);
        lua_pushvalue(L, resultIdx);
        lua_pushvalue(L, chainedRefsTableIdx);
        lua_pushcclosurek(
            L,
            [](lua_State* L) -> int {
                int idx = (int)lua_tointeger(L, lua_upvalueindex(1));
                // upvalue 2 = state table, 3 = result promise, 4 = chainedRefs

                lua_pushvalue(L, lua_upvalueindex(2)); // state
                int st = lua_gettop(L);

                lua_getfield(L, st, "done");
                if (lua_toboolean(L, -1))
                {
                    lua_pop(L, 2);
                    return 0;
                }
                lua_pop(L, 1);

                // Store result at index.
                lua_getfield(L, st, "results");
                lua_pushvalue(L, 1);
                lua_rawseti(L, -2, idx);
                lua_pop(L, 1);

                // Decrement remaining.
                lua_getfield(L, st, "remaining");
                int rem = (int)lua_tointeger(L, -1) - 1;
                lua_pop(L, 1);
                lua_pushinteger(L, rem);
                lua_setfield(L, st, "remaining");

                if (rem == 0)
                {
                    lua_pushboolean(L, true);
                    lua_setfield(L, st, "done");

                    auto* rp = lua_torive<ScriptedPromise>(L,
                                                           lua_upvalueindex(3),
                                                           true);
                    if (rp && rp->isPending())
                    {
                        lua_getfield(L, st, "results");
                        rp->resolve(L, lua_gettop(L));
                        lua_pop(L, 1);
                    }

                    // Clean up cancelCollectionRefs in chainedRefs table.
                    lua_pushvalue(L, lua_upvalueindex(4));
                    int tbl = lua_gettop(L);
                    int count = (int)lua_objlen(L, tbl);
                    for (int j = 1; j <= count; j++)
                    {
                        lua_rawgeti(L, tbl, j);
                        int cRef = (int)lua_tointeger(L, -1);
                        lua_pop(L, 1);
                        if (cRef != LUA_NOREF)
                            lua_unref(L, cRef);
                    }
                    lua_pop(L, 1); // pop table
                }

                lua_pop(L, 1); // pop state table
                return 0;
            },
            nullptr,
            4,
            nullptr);
        int successRef = lua_ref(L, -1);
        lua_pop(L, 1);

        // Failure closure: upvalues = (state, resultPromise, chainedRefs)
        lua_pushvalue(L, stateIdx);
        lua_pushvalue(L, resultIdx);
        lua_pushvalue(L, chainedRefsTableIdx);
        lua_pushcclosurek(
            L,
            [](lua_State* L) -> int {
                // upvalue 1 = state, 2 = result promise, 3 = chainedRefs
                lua_pushvalue(L, lua_upvalueindex(1)); // state
                int st = lua_gettop(L);

                lua_getfield(L, st, "done");
                if (lua_toboolean(L, -1))
                {
                    lua_pop(L, 2);
                    return 0;
                }
                lua_pop(L, 1);

                lua_pushboolean(L, true);
                lua_setfield(L, st, "done");

                auto* rp =
                    lua_torive<ScriptedPromise>(L, lua_upvalueindex(2), true);
                if (rp && rp->isPending())
                {
                    rp->reject(L, 1); // arg 1 is the error
                }

                // Cancel all chained promises from chainedRefs table.
                lua_pushvalue(L, lua_upvalueindex(3));
                int tbl = lua_gettop(L);
                int count = (int)lua_objlen(L, tbl);
                for (int j = 1; j <= count; j++)
                {
                    lua_rawgeti(L, tbl, j);
                    int cRef = (int)lua_tointeger(L, -1);
                    lua_pop(L, 1);
                    if (cRef != LUA_NOREF)
                    {
                        lua_rawgeti(L, LUA_REGISTRYINDEX, cRef);
                        auto* cp = lua_torive<ScriptedPromise>(L, -1, true);
                        lua_pop(L, 1);
                        if (cp && cp->isPending())
                            cp->cancel(L);
                        lua_unref(L, cRef);
                    }
                }
                lua_pop(L, 1); // pop table

                lua_pop(L, 1); // pop state table
                return 0;
            },
            nullptr,
            3,
            nullptr);
        int failureRef = lua_ref(L, -1);
        lua_pop(L, 1);

        {
            ScriptedPromise::ThenCallback cb;
            cb.successRef = successRef;
            cb.failureRef = failureRef;
            cb.chainedPromiseRef = chainedRef;
            pi->m_thenCallbacks.push_back(cb);
        }

        if (!pi->isPending())
            promise_notifyCallbacks(L, pi);

        lua_pop(L, 2); // pop chained promise and pi
    }

    // Register onCancel hook: captures chainedRefs table as direct upvalue.
    lua_pushvalue(L, chainedRefsTableIdx);
    lua_pushcclosurek(
        L,
        [](lua_State* L) -> int {
            // upvalue 1 = chainedRefs table
            lua_pushvalue(L, lua_upvalueindex(1));
            int tbl = lua_gettop(L);
            int count = (int)lua_objlen(L, tbl);

            for (int j = 1; j <= count; j++)
            {
                lua_rawgeti(L, tbl, j);
                int cRef = (int)lua_tointeger(L, -1);
                lua_pop(L, 1);
                if (cRef != LUA_NOREF)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, cRef);
                    auto* cp = lua_torive<ScriptedPromise>(L, -1, true);
                    lua_pop(L, 1);
                    if (cp && cp->isPending())
                        cp->cancel(L);
                    lua_unref(L, cRef);
                }
            }
            lua_pop(L, 1); // pop table
            return 0;
        },
        nullptr,
        1,
        nullptr);
    result->m_onCancelRef = lua_ref(L, -1);
    lua_pop(L, 1);

    // Pop the chainedRefs table and state table from the stack.
    lua_pop(L, 2);

    lua_pushvalue(L, resultIdx);
    return 1;
}

// ── Promise table __index (static method dispatch) ─────────────────────

static int promise_table_index(lua_State* L)
{
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "resolve") == 0)
    {
        lua_pushcfunction(L, promise_static_resolve, "Promise.resolve");
        return 1;
    }
    if (strcmp(key, "reject") == 0)
    {
        lua_pushcfunction(L, promise_static_reject, "Promise.reject");
        return 1;
    }
    if (strcmp(key, "all") == 0)
    {
        lua_pushcfunction(L, promise_static_all, "Promise.all");
        return 1;
    }
    if (strcmp(key, "new") == 0)
    {
        lua_pushcfunction(L, promise_static_new, "Promise.new");
        return 1;
    }
    luaL_error(L, "'%s' is not a valid member of Promise", key);
    return 0;
}

// ── async / await ──────────────────────────────────────────────────────────

// await(promise) -> (ok: boolean, value: any)
// Non-throwing: returns (true, resolvedValue) or (false, rejectionReason).
// Matches Roblox Promise:await() semantics. Use expect() for throwing variant.
static int lua_await(lua_State* L)
{
    auto* p = lua_torive<ScriptedPromise>(L, 1);

    if (!lua_isyieldable(L))
    {
        luaL_error(L, "await() must be called inside async()");
        return 0;
    }

    if (p->isFulfilled())
    {
        lua_pushboolean(L, true);
        lua_rawgeti(L, LUA_REGISTRYINDEX, p->resultRef());
        return 2;
    }
    if (p->isRejected())
    {
        lua_pushboolean(L, false);
        lua_rawgeti(L, LUA_REGISTRYINDEX, p->resultRef());
        return 2;
    }
    if (p->isCancelled())
    {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Promise was cancelled");
        return 2;
    }

    // Pending — yield with the promise on the stack so the caller
    // (async's resume callback) can attach andThen to it.
    lua_pushvalue(L, 1);
    int nresults = lua_yield(L, 1);
    // After resume: success callback pushes (true, value),
    // failure callback pushes (false, error).
    // Return them directly — no throwing.
    return nresults;
}

static int lua_async(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    // Create the result promise.
    lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
    int promiseIdx = lua_gettop(L);

    // Create a coroutine. Both stay on L's stack — closures capture
    // them as direct upvalues, GC keeps them alive.
    lua_State* co = lua_newthread(L);
    int coIdx = lua_gettop(L);

    // Copy function to coroutine stack.
    lua_pushvalue(L, 1);
    lua_xmove(L, co, 1);

    // Resume the coroutine (0 args).
    int status = lua_resume(co, L, 0);

    handleCoroutineCompletion(L, co, status, coIdx, promiseIdx);

    // Clean up thread from main stack.
    lua_remove(L, coIdx);

    // Return the promise.
    lua_pushvalue(L, promiseIdx);
    return 1;
}

// handleCoroutineCompletion takes the coroutine and async promise as
// stack values (coIdx, asyncPromiseIdx on L). Closures capture them
// as direct upvalues — no registry refs needed; GC owns lifetime.
static void handleCoroutineCompletion(lua_State* L,
                                      lua_State* co,
                                      int status,
                                      int coIdx,
                                      int asyncPromiseIdx)
{
    auto* asyncPromise = lua_torive<ScriptedPromise>(L, asyncPromiseIdx, true);

    if (status == LUA_OK)
    {
        // Coroutine finished — resolve with the return value (if any).
        // Use selfRef overload for A+ flattening (user may return a Promise).
        if (lua_gettop(co) > 0)
        {
            lua_xmove(co, L, 1);
            lua_pushvalue(L, asyncPromiseIdx);
            int selfRef = lua_ref(L, -1);
            lua_pop(L, 1);
            asyncPromise->resolve(L, lua_gettop(L), selfRef);
            lua_unref(L, selfRef);
            lua_pop(L, 1);
        }
        else
        {
            lua_pushnil(L);
            asyncPromise->resolve(L, lua_gettop(L));
            lua_pop(L, 1);
        }
        // No refs to clean up — GC handles the upvalues.
    }
    else if (status == LUA_YIELD)
    {
        // Coroutine yielded at await(promise).
        if (lua_gettop(co) < 1)
        {
            luaL_error(L, "async: coroutine yielded without a promise");
            return;
        }

        auto* awaited = lua_torive<ScriptedPromise>(co, -1, true);
        if (!awaited)
        {
            luaL_error(L, "async: await() argument is not a Promise");
            return;
        }
        lua_pop(co, 1); // pop the promise from co stack

        // Success callback: captures coroutine + asyncPromise as upvalues.
        // asyncPromise is captured to prevent GC, not used directly.
        lua_pushvalue(L, coIdx);
        lua_pushvalue(L, asyncPromiseIdx);
        lua_pushcclosurek(
            L,
            [](lua_State* L) -> int {
                lua_State* co = lua_tothread(L, lua_upvalueindex(1));

                // Push (true, resolvedValue) onto coroutine stack.
                lua_pushboolean(co, true);
                lua_pushvalue(L, 1);
                lua_xmove(L, co, 1);

                int status = lua_resume(co, L, 2);

                // Push co + asyncPromise onto stack for recursive call.
                lua_pushvalue(L, lua_upvalueindex(1));
                int coIdx = lua_gettop(L);
                lua_pushvalue(L, lua_upvalueindex(2));
                int apIdx = lua_gettop(L);
                handleCoroutineCompletion(L, co, status, coIdx, apIdx);
                lua_pop(L, 2);
                return 0;
            },
            nullptr,
            2,
            nullptr);
        int successRef = lua_ref(L, -1);
        lua_pop(L, 1);

        // Failure callback: resume coroutine with (false, error).
        // asyncPromise is captured to prevent GC, not used directly.
        lua_pushvalue(L, coIdx);
        lua_pushvalue(L, asyncPromiseIdx);
        lua_pushcclosurek(
            L,
            [](lua_State* L) -> int {
                lua_State* co = lua_tothread(L, lua_upvalueindex(1));

                lua_pushboolean(co, false);
                lua_pushvalue(L, 1);
                lua_xmove(L, co, 1);

                int status = lua_resume(co, L, 2);

                lua_pushvalue(L, lua_upvalueindex(1));
                int coIdx = lua_gettop(L);
                lua_pushvalue(L, lua_upvalueindex(2));
                int apIdx = lua_gettop(L);
                handleCoroutineCompletion(L, co, status, coIdx, apIdx);
                lua_pop(L, 2);
                return 0;
            },
            nullptr,
            2,
            nullptr);
        int failureRef = lua_ref(L, -1);
        lua_pop(L, 1);

        // Cancel callback: close the coroutine and reject async promise.
        lua_pushvalue(L, coIdx);
        lua_pushvalue(L, asyncPromiseIdx);
        lua_pushcclosurek(
            L,
            [](lua_State* L) -> int {
                lua_State* co = lua_tothread(L, lua_upvalueindex(1));
                auto* ap =
                    lua_torive<ScriptedPromise>(L, lua_upvalueindex(2), true);

                // Close the coroutine to release its stack/locals
                // immediately rather than waiting for GC.
                if (co)
                    lua_resetthread(co);

                if (ap && ap->isPending())
                {
                    lua_pushstring(L, "Promise was cancelled");
                    ap->reject(L, lua_gettop(L));
                    lua_pop(L, 1);
                }
                return 0;
            },
            nullptr,
            2,
            nullptr);
        int cancelRef = lua_ref(L, -1);
        lua_pop(L, 1);

        {
            ScriptedPromise::ThenCallback cb;
            cb.successRef = successRef;
            cb.failureRef = failureRef;
            cb.cancelRef = cancelRef;
            awaited->m_thenCallbacks.push_back(cb);
        }

        if (!awaited->isPending())
            promise_notifyCallbacks(L, awaited);
    }
    else
    {
        // Coroutine errored.
        if (lua_gettop(co) > 0)
        {
            lua_xmove(co, L, 1);
            asyncPromise->reject(L, lua_gettop(L));
            lua_pop(L, 1);
        }
        else
        {
            lua_pushstring(L, "async function errored");
            asyncPromise->reject(L, lua_gettop(L));
            lua_pop(L, 1);
        }
    }
}

// ── registration ───────────────────────────────────────────────────────────

int luaopen_rive_promise(lua_State* L)
{
    // Register ScriptedPromise type.
    lua_register_rive<ScriptedPromise>(L);
    lua_pushcfunction(L, promise_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop metatable

    // Create global Promise table.
    lua_newtable(L);

    // Metatable for static method dispatch.
    lua_newtable(L);
    lua_pushcfunction(L, promise_table_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);

    lua_setglobal(L, "Promise");

    // Register global await / async functions.
    lua_pushcfunction(L, lua_await, "await");
    lua_setglobal(L, "await");

    lua_pushcfunction(L, lua_async, "async");
    lua_setglobal(L, "async");

    return 0;
}

} // namespace rive

#endif // WITH_RIVE_SCRIPTING
