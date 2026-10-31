// A GPU canvas a script makes before its recording session has a device. Web
// binds the replay worker's device at the first frame, after scripts init, so
// the canvas has to be minted from the session alone.

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "utils/no_op_factory.hpp"

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "rive/renderer/cmd/deferred_session.hpp"

using namespace rive;

namespace
{
bool runInit(ScriptingTest& vm)
{
  ScriptedObjectTest scriptedObjectTest;
  lua_State* L = vm.state();
  lua_getglobal(L, "init");
  lua_pushvalue(L, -2);
  lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK)
  {
    FAIL(lua_tostring(L, -1));
  }
  return lua_toboolean(L, -1);
}
} // namespace

TEST_CASE(
    "a gpu canvas made before the device binds has its backing and view",
    "[scripting][deferred]")
{
  cmd::DeferredSession session(ore::ReplayCaps{});
  REQUIRE(session.renderContext() == nullptr);
  ScriptingTest vm(
      R"(
function init(self, context)
  local canvas = context:gpuCanvas({ width = 64, height = 32 })
  assert(canvas.width == 64 and canvas.height == 32, 'size')
  assert(canvas.image ~= nil, 'image')
  assert(canvas:colorView().format == 'rgba8unorm', 'view')
  return true
end
)",
      1,
      false,
      {},
      true,
      &session);
  CHECK(runInit(vm));
}

TEST_CASE(
    "a gpu canvas resized before the device binds takes the new size",
    "[scripting][deferred]")
{
  cmd::DeferredSession session(ore::ReplayCaps{});
  ScriptingTest vm(
      R"(
function init(self, context)
  local canvas = context:gpuCanvas()
  canvas:resize(128, 16)
  assert(canvas.width == 128 and canvas.height == 16, 'size')
  local first = canvas.image
  assert(first ~= nil and canvas:colorView() ~= nil, 'backing')
  canvas:resize(8, 8)
  assert(canvas.width == 8 and canvas.height == 8, 'resized')
  assert(canvas.image ~= first, 'new backing')
  return true
end
)",
      1,
      false,
      {},
      true,
      &session);
  CHECK(runInit(vm));
}

TEST_CASE(
    "an immediate gpu canvas still needs a render context",
    "[scripting][deferred]")
{
  NoOpFactory factory;
  ScriptingTest vm(
      R"(
function init(self, context)
  local ok, err = pcall(function()
    context:gpuCanvas({ width = 4, height = 4 })
  end)
  return not ok and string.find(err, 'RenderContext') ~= nil
end
)",
      1,
      false,
      {},
      true,
      &factory);
  CHECK(runInit(vm));
}
#endif
