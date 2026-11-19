#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/animation/listener_invocation.hpp"

using namespace rive;

namespace
{
// Calls the function the source returns with `invocation` as a listener
// action's context, the way callListenerPerform dispatches it, and returns
// the hit() the script gave the event it took from the context.
HitResult perform(const char* source, ListenerInvocation invocation)
{
  ScriptingTest vm(source);
  lua_State* L = vm.state();
  // Stack: [performAction]
  auto scripted = rive_lua_push_scripted_invocation(L, invocation);
  lua_pushvalue(L, -1);
  lua_insert(L, -3);
  // Stack: [context, performAction, context]
  CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
  HitResult hitResult = rive_lua_end_scripted_invocation(L, scripted);
  lua_pop(L, 1);
  return hitResult;
}

ListenerInvocation emoji()
{
  return ListenerInvocation::textInput("\xF0\x9F\x98\x80");
}

ListenerInvocation escapePress()
{
  return ListenerInvocation::keyboard(
      Key::escape,
      KeyModifiers::none,
      true,
      false);
}
} // namespace

TEST_CASE(
    "a listener action's hit() on its text event is read back",
    "[scripting]")
{
  const char* blockEmoji = R"(
return function(ctx)
  local input = ctx:asTextInput()
  for _, codepoint in utf8.codes(input.text) do
    if codepoint >= 0x1F000 then
      input:hit()
    end
  end
end
)";
  CHECK(perform(blockEmoji, emoji()) == HitResult::hitOpaque);
  CHECK(
      perform(blockEmoji, ListenerInvocation::textInput("a")) ==
      HitResult::none);
  // hit(true) only marks the text seen.
  CHECK(
      perform(
          "return function(ctx) ctx:asTextInput():hit(true) end",
          emoji()) == HitResult::hit);
}

TEST_CASE(
    "a listener action's hit() on its key event is read back",
    "[scripting]")
{
  CHECK(
      perform(
          "return function(ctx) ctx:asKeyboardEvent():hit() end",
          escapePress()) == HitResult::hitOpaque);
  CHECK(
      perform(
          "return function(ctx) ctx:asKeyboardEvent():hit(true) end",
          escapePress()) == HitResult::hit);
  CHECK(
      perform(
          "return function(ctx) local _ = ctx:asKeyboardEvent().key end",
          escapePress()) == HitResult::none);
}

TEST_CASE(
    "asKeyboardEvent and asTextInput hand back one event per dispatch",
    "[scripting]")
{
  // A hit() on the event from any call counts, so every call returns the
  // same one.
  CHECK(
      perform(
          R"(
return function(ctx)
  assert(rawequal(ctx:asKeyboardEvent(), ctx:asKeyboardEvent()))
  ctx:asKeyboardEvent():hit()
end
)",
          escapePress()) == HitResult::hitOpaque);
  CHECK(
      perform(
          R"(
return function(ctx)
  assert(rawequal(ctx:asTextInput(), ctx:asTextInput()))
  ctx:asTextInput():hit()
end
)",
          emoji()) == HitResult::hitOpaque);
}

TEST_CASE("a node script's hit() in textEvent stops the text", "[scripting]")
{
  ScriptingTest vm(R"(
return {
  textEvent = function(self, event)
    if event.text == 'stop' then
      event:hit()
    end
    return event.text == 'return'
  end,
  keyboardEvent = function(self, event)
    event:hit(event.key == 256)
    return false
  end,
}
)");
  lua_State* L = vm.state();
  int self = refTopFunction(L);
  ScriptingVM* scripting = vm.vm();
  CHECK(scripting->callTextEvent(nullptr, self, "stop"));
  CHECK(scripting->callTextEvent(nullptr, self, "return"));
  CHECK_FALSE(scripting->callTextEvent(nullptr, self, "go"));
  // hit(true) leaves the key going; an opaque hit() stops it.
  CHECK_FALSE(scripting->callKeyboardEvent(
      nullptr,
      self,
      Key::escape,
      KeyModifiers::none,
      true,
      false));
  CHECK(scripting->callKeyboardEvent(
      nullptr,
      self,
      Key::a,
      KeyModifiers::none,
      true,
      false));
  CHECK(lua_gettop(L) == 0);
  lua_unref(L, self);
}
