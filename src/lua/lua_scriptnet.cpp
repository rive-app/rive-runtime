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
// A promise on the stack, settled with the value under it.
int pushResolved(lua_State* L)
{
  int valueIdx = lua_gettop(L);
  lua_newrive<ScriptedPromise>(L, lua_mainthread(L))->resolve(L, valueIdx);
  return 1;
}

int pushRejected(lua_State* L, const char* message)
{
  lua_pushstring(L, message);
  int errorIdx = lua_gettop(L);
  lua_newrive<ScriptedPromise>(L, lua_mainthread(L))->reject(L, errorIdx);
  return 1;
}

// What a reader's read() resolves with: { done, value }.
void pushRead(lua_State* L, const std::vector<uint8_t>* bytes)
{
  lua_createtable(L, 0, 2);
  lua_pushboolean(L, bytes == nullptr);
  lua_setfield(L, -2, "done");
  if (bytes != nullptr)
  {
    memcpy(lua_newbuffer(L, bytes->size()), bytes->data(), bytes->size());
    lua_setfield(L, -2, "value");
  }
}

// The registered promise if it is still pending, on the stack; else nothing.
ScriptedPromise* pushPendingPromise(lua_State* L, int ref)
{
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  auto* promise = lua_torive<ScriptedPromise>(L, -1, true);
  if (promise == nullptr || !promise->isPending())
  {
    lua_pop(L, 1);
    return nullptr;
  }
  return promise;
}

std::string rejection(const scriptnet::NetError& error)
{
  return std::string(scriptnet::netErrorCodeName(error.code)) + ": " +
         error.message;
}

// The most bytes one Luau string or buffer holds.
constexpr size_t kMaxLuauBlock = 1 << 30;
} // namespace

namespace rive
{
// One script's fetch. It settles the fetch's promise with the response, then
// is that response's body: read whole by text() and arrayBuffer(), or a run
// at a time by a reader. It holds the VM's main lua_State until the VM
// closes or the fetch is cancelled.
class ScriptedFetchBody : public scriptnet::FetchListener
{
public:
  ScriptedFetchBody(lua_State* state, int promiseRef, uint64_t ownerId)
      : m_state(state)
      , m_promiseRef(promiseRef)
      , m_ownerId(ownerId)
  {}

  /// Frees the responses a script dropped unread when they hold back
  /// requests of `ownerId`. Twice over: a settled promise keeps its response
  /// until it is collected itself.
  static void collectUnread(lua_State* L, uint64_t ownerId)
  {
    if (scriptnet::waitsOnUnreadBodies(ownerId))
    {
      lua_gc(L, LUA_GCCOLLECT, 0);
      lua_gc(L, LUA_GCCOLLECT, 0);
    }
  }

  /// The request, once fetch() has given it.
  scriptnet::RequestId id = 0;
  /// A reader has the body, so nothing else reads it.
  bool locked = false;
  /// Script objects that can still read the body.
  int holds = 0;

  bool streams() const override { return true; }

  void onResponse(scriptnet::HttpResponse&& response) override
  {
    lua_State* L = m_state;
    if (L == nullptr)
    {
      return;
    }
    bool settled = false;
    if (ScriptedPromise* promise = pushPendingPromise(L, m_promiseRef))
    {
      lua_newrive<ScriptedHttpResponse>(L, std::move(response), this);
      promise->resolve(L, lua_gettop(L));
      lua_pop(L, 2);
      settled = true;
    }
    lua_unref(L, m_promiseRef);
    m_promiseRef = LUA_NOREF;
    if (settled)
    {
      ScriptingContext::asyncWorkDelivered(L);
    }
    unwanted();
    // A script that only wanted the status has dropped it by now.
    collectUnread(L, m_ownerId);
  }

  void onError(const scriptnet::NetError& error) override
  {
    lua_State* L = m_state;
    if (L == nullptr)
    {
      return;
    }
    m_done = true;
    bool settled = false;
    if (ScriptedPromise* promise = pushPendingPromise(L, m_promiseRef))
    {
      lua_pushfstring(
          L,
          "%s: %s",
          scriptnet::netErrorCodeName(error.code),
          error.message.c_str());
      promise->reject(L, lua_gettop(L));
      lua_pop(L, 2);
      settled = true;
    }
    lua_unref(L, m_promiseRef);
    m_promiseRef = LUA_NOREF;
    m_state = nullptr;
    if (settled)
    {
      ScriptingContext::asyncWorkDelivered(L);
    }
  }

  // The script cancelled the promise, whose cancel hook releases the ref,
  // or the VM is closing and Lua must not be touched.
  void onCancel() override
  {
    if (m_cancelling)
    {
      return;
    }
    m_done = true;
    m_error = "aborted: the request was cancelled";
    m_state = nullptr;
    m_promiseRef = LUA_NOREF;
    m_readRef = LUA_NOREF;
    m_waiters.clear();
  }

  void onBody() override
  {
    lua_State* L = m_state;
    if (L == nullptr)
    {
      return;
    }
    // With nothing waiting, what arrived stays for whoever asks next.
    if (m_gathers ? !m_waiters.empty() && gather(L)
                  : m_readRef != LUA_NOREF && answerRead(L))
    {
      ScriptingContext::asyncWorkDelivered(L);
    }
    unwanted();
  }

  /// text() and arrayBuffer(): a promise of the whole body, on the stack.
  int whole(lua_State* L, bool text)
  {
    if (m_whole)
    {
      pushWhole(L, text);
      return pushResolved(L);
    }
    if (!m_error.empty())
    {
      return pushRejected(L, m_error.c_str());
    }
    if (locked || m_done)
    {
      return pushRejected(
          L,
          "aborted: the body was read through its reader or cancelled");
    }
    m_gathers = true;
    lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
    int promiseIdx = lua_gettop(L);
    lua_pushvalue(L, promiseIdx);
    int ref = lua_ref(L, -1);
    lua_pop(L, 1);
    m_waiters.push_back({ref, text});
    hookCancel(L, promiseIdx, ref);
    gather(L);
    lua_settop(L, promiseIdx);
    return 1;
  }

  bool gathers() const { return m_gathers; }

  /// A reader's read(): a promise of the next run, on the stack.
  int read(lua_State* L)
  {
    if (m_readRef != LUA_NOREF)
    {
      return pushRejected(L, "a read is already waiting for this body");
    }
    if (!m_error.empty())
    {
      return pushRejected(L, m_error.c_str());
    }
    if (m_done)
    {
      pushRead(L, nullptr);
      return pushResolved(L);
    }
    scriptnet::NetError error;
    switch (scriptnet::readBody(id, &m_run, &error))
    {
      case scriptnet::BodyState::Data:
        pushRead(L, &m_run);
        m_run.clear();
        return pushResolved(L);
      case scriptnet::BodyState::Pending:
        lua_newrive<ScriptedPromise>(L, lua_mainthread(L));
        lua_pushvalue(L, -1);
        m_readRef = lua_ref(L, -1);
        lua_pop(L, 1);
        hookCancel(L, lua_gettop(L), m_readRef);
        return 1;
      case scriptnet::BodyState::End:
        m_done = true;
        pushRead(L, nullptr);
        return pushResolved(L);
      case scriptnet::BodyState::Failed:
        break;
    }
    m_done = true;
    m_error = rejection(error);
    return pushRejected(L, m_error.c_str());
  }

  /// A reader's cancel(): stops the download, and a read waiting ends.
  void cancelRead(lua_State* L)
  {
    stop();
    if (m_readRef == LUA_NOREF)
    {
      return;
    }
    int ref = m_readRef;
    m_readRef = LUA_NOREF;
    if (ScriptedPromise* promise = pushPendingPromise(L, ref))
    {
      pushRead(L, nullptr);
      promise->resolve(L, lua_gettop(L));
      lua_pop(L, 2);
    }
    lua_unref(L, ref);
  }

  /// Cancels a body still arriving that no script object or promise can
  /// read any more. Reached from the collector, so it leaves Lua alone.
  void unwanted()
  {
    if (holds == 0 && m_waiters.empty() && m_readRef == LUA_NOREF)
    {
      stop();
    }
  }

private:
  struct Waiter
  {
    int ref;
    bool text;
  };

  // Keeps the body for as long as a promise that can be cancelled.
  struct Keep
  {
    rcp<ScriptedFetchBody> body;
  };

  // Has the promise at `promiseIdx`, registered as `ref`, tell the body when
  // a script cancels it.
  void hookCancel(lua_State* L, int promiseIdx, int ref)
  {
    auto* promise = lua_torive<ScriptedPromise>(L, promiseIdx);
    auto* keep = static_cast<Keep*>(
        lua_newuserdatadtor(L, sizeof(Keep), [](void* p) {
          static_cast<Keep*>(p)->~Keep();
        }));
    new (keep) Keep{ref_rcp(this)};
    lua_pushinteger(L, ref);
    lua_pushcclosurek(
        L,
        [](lua_State* L) -> int {
          auto* keep = static_cast<Keep*>(
              lua_touserdata(L, lua_upvalueindex(1)));
          keep->body->cancelled(L, (int)lua_tointeger(L, lua_upvalueindex(2)));
          return 0;
        },
        nullptr,
        2,
        nullptr);
    promise->m_onCancelRef = lua_ref(L, -1);
    lua_pop(L, 1);
  }

  // A script cancelled the promise it read the body with, so that promise
  // no longer keeps the body, and what arrives waits for the next read.
  void cancelled(lua_State* L, int ref)
  {
    if (ref == m_readRef)
    {
      m_readRef = LUA_NOREF;
    }
    else
    {
      size_t at = 0;
      while (at < m_waiters.size() && m_waiters[at].ref != ref)
      {
        at++;
      }
      if (at == m_waiters.size())
      {
        return;
      }
      m_waiters.erase(m_waiters.begin() + at);
    }
    lua_unref(L, ref);
    if (m_readRef == LUA_NOREF && m_waiters.empty() && !m_done)
    {
      scriptnet::forgetRead(id);
    }
    unwanted();
  }

  // Ends the body where it is, aborting what has yet to arrive.
  void stop()
  {
    if (m_done)
    {
      return;
    }
    m_done = true;
    // Cancelling tells this listener, which must keep its state.
    m_cancelling = true;
    scriptnet::cancel(id);
    m_cancelling = false;
  }

  // Takes what has arrived for text() and arrayBuffer(), and settles their
  // promises once the body is whole or has failed. True when it did.
  bool gather(lua_State* L)
  {
    size_t limit = std::min<size_t>(
        scriptnet::limits().maxResponseBodyBytes,
        kMaxLuauBlock);
    for (;;)
    {
      scriptnet::NetError error;
      scriptnet::BodyState state = scriptnet::readBody(id, &m_run, &error);
      if (state == scriptnet::BodyState::Pending)
      {
        return false;
      }
      if (state == scriptnet::BodyState::Data)
      {
        m_bytes += m_run.size();
        m_parts.push_back(std::move(m_run));
        m_run.clear();
        if (m_bytes <= limit)
        {
          continue;
        }
        stop();
        error = {
            scriptnet::NetErrorCode::TooLarge,
            "the response body is larger than " + std::to_string(limit) +
                " bytes"};
        state = scriptnet::BodyState::Failed;
      }
      m_done = true;
      if (state == scriptnet::BodyState::Failed)
      {
        m_error = rejection(error);
        m_parts.clear();
        m_bytes = 0;
      }
      else
      {
        m_whole = true;
      }
      break;
    }
    for (size_t i = 0; i < m_waiters.size(); i++)
    {
      Waiter waiter = m_waiters[i];
      if (ScriptedPromise* promise = pushPendingPromise(L, waiter.ref))
      {
        if (m_whole)
        {
          pushWhole(L, waiter.text);
          promise->resolve(L, lua_gettop(L));
        }
        else
        {
          lua_pushlstring(L, m_error.data(), m_error.size());
          promise->reject(L, lua_gettop(L));
        }
        lua_pop(L, 2);
      }
      lua_unref(L, waiter.ref);
    }
    m_waiters.clear();
    return true;
  }

  // Settles the read a reader waits on, once there is something to tell it.
  bool answerRead(lua_State* L)
  {
    scriptnet::NetError error;
    scriptnet::BodyState state = scriptnet::readBody(id, &m_run, &error);
    if (state == scriptnet::BodyState::Pending)
    {
      return false;
    }
    if (state != scriptnet::BodyState::Data)
    {
      m_done = true;
    }
    if (state == scriptnet::BodyState::Failed)
    {
      m_error = rejection(error);
    }
    int ref = m_readRef;
    m_readRef = LUA_NOREF;
    if (ScriptedPromise* promise = pushPendingPromise(L, ref))
    {
      if (state == scriptnet::BodyState::Failed)
      {
        lua_pushlstring(L, m_error.data(), m_error.size());
        promise->reject(L, lua_gettop(L));
      }
      else
      {
        pushRead(L, state == scriptnet::BodyState::Data ? &m_run : nullptr);
        promise->resolve(L, lua_gettop(L));
      }
      lua_pop(L, 2);
    }
    m_run.clear();
    lua_unref(L, ref);
    return true;
  }

  // The whole body as a string or a buffer, on the stack.
  void pushWhole(lua_State* L, bool text)
  {
    if (text)
    {
      luaL_Strbuf buffer;
      luaL_buffinitsize(L, &buffer, m_bytes);
      for (const std::vector<uint8_t>& part : m_parts)
      {
        luaL_addlstring(&buffer, (const char*)part.data(), part.size());
      }
      luaL_pushresult(&buffer);
      return;
    }
    uint8_t* out = (uint8_t*)lua_newbuffer(L, m_bytes);
    for (const std::vector<uint8_t>& part : m_parts)
    {
      memcpy(out, part.data(), part.size());
      out += part.size();
    }
  }

  lua_State* m_state;
  int m_promiseRef;
  uint64_t m_ownerId;
  // The body has ended, failed or been cancelled: nothing more arrives.
  bool m_done = false;
  bool m_cancelling = false;
  // Why the body failed, as scripts hear it.
  std::string m_error;
  // The run a read is about to hand over.
  std::vector<uint8_t> m_run;
  // The read a reader waits on.
  int m_readRef = LUA_NOREF;
  // text() or arrayBuffer() asked for the whole body: what has arrived of
  // it, kept so each can be asked again.
  bool m_gathers = false;
  bool m_whole = false;
  std::vector<std::vector<uint8_t>> m_parts;
  size_t m_bytes = 0;
  std::vector<Waiter> m_waiters;
};

ScriptedFetchBodyHold::ScriptedFetchBodyHold(ScriptedFetchBody* body)
    : body(body)
{
  body->ref();
  body->holds++;
}

ScriptedFetchBodyHold::~ScriptedFetchBodyHold()
{
  body->holds--;
  body->unwanted();
  body->unref();
}
} // namespace rive

namespace
{

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
      if (lua_type(L, -2) != LUA_TSTRING || lua_type(L, -1) != LUA_TSTRING)
      {
        luaL_error(
            L,
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
    luaL_error(
        L,
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
      request.headers.push_back(
          {std::string(name, nameLength), std::string(value, valueLength)});
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
    request.timeoutMs = (uint32_t)std::min(lua_tonumber(L, -1), 4294967295.0);
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
    auto body = make_rcp<ScriptedFetchBody>(mainThread, promiseRef, ownerId);
    id = scriptnet::fetch(
        ownerId,
        std::move(request),
        body,
        context->cloudEndpoint());
    body->id = id;
  }
  ScriptedFetchBody::collectUnread(L, ownerId);

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

int pushHeader(
    lua_State* L,
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
      lua_pushlstring(
          L,
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
    case (int)LuaAtoms::body:
      lua_newrive<ScriptedReadableStream>(L, self->body.body);
      return 1;
    default:
      break;
  }
  luaL_error(
      L,
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
    switch (atom)
    {
      case (int)LuaAtoms::text:
        return self->body.body->whole(L, true);
      case (int)LuaAtoms::arrayBuffer:
        return self->body.body->whole(L, false);
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
  luaL_error(
      L,
      "%s is not a valid method of %s",
      name != nullptr ? name : "?",
      ScriptedHttpResponse::luaName);
  return 0;
}
// ── ReadableStream and its reader ──────────────────────────────────────────

int streamIndex(lua_State* L)
{
  auto* self = lua_torive<ScriptedReadableStream>(L, 1);
  int atom;
  const char* key = lua_tostringatom(L, 2, &atom);
  if (key != nullptr && atom == (int)LuaAtoms::locked)
  {
    lua_pushboolean(L, self->body.body->locked);
    return 1;
  }
  luaL_error(
      L,
      "'%s' is not a valid index of %s",
      key != nullptr ? key : "?",
      ScriptedReadableStream::luaName);
  return 0;
}

int streamNamecall(lua_State* L)
{
  int atom;
  const char* name = lua_namecallatom(L, &atom);
  auto* self = lua_torive<ScriptedReadableStream>(L, 1);
  if (name != nullptr && atom == (int)LuaAtoms::getReader)
  {
    ScriptedFetchBody* body = self->body.body;
    if (body->locked)
    {
      luaL_error(L, "getReader: this body already has a reader");
    }
    if (body->gathers())
    {
      luaL_error(L, "getReader: this body is being read whole");
    }
    body->locked = true;
    lua_newrive<ScriptedStreamReader>(L, body);
    return 1;
  }
  luaL_error(
      L,
      "%s is not a valid method of %s",
      name != nullptr ? name : "?",
      ScriptedReadableStream::luaName);
  return 0;
}

int readerNamecall(lua_State* L)
{
  int atom;
  const char* name = lua_namecallatom(L, &atom);
  auto* self = lua_torive<ScriptedStreamReader>(L, 1);
  if (name != nullptr)
  {
    switch (atom)
    {
      case (int)LuaAtoms::read:
        return self->body.body->read(L);
      case (int)LuaAtoms::cancel:
        self->body.body->cancelRead(L);
        return 0;
      default:
        break;
    }
  }
  luaL_error(
      L,
      "%s is not a valid method of %s",
      name != nullptr ? name : "?",
      ScriptedStreamReader::luaName);
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

  lua_register_rive<ScriptedReadableStream>(L);
  lua_pushcfunction(L, streamIndex, nullptr);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, streamNamecall, nullptr);
  lua_setfield(L, -2, "__namecall");
  lua_setreadonly(L, -1, true);
  lua_pop(L, 1);

  lua_register_rive<ScriptedStreamReader>(L);
  lua_pushcfunction(L, readerNamecall, nullptr);
  lua_setfield(L, -2, "__namecall");
  lua_setreadonly(L, -1, true);
  lua_pop(L, 1);

  lua_pushcfunction(L, scriptnetFetch, "fetch");
  lua_setglobal(L, "fetch");
  return 0;
}

#endif // WITH_RIVE_SCRIPTING && WITH_RIVE_SCRIPTNET
