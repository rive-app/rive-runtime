// Canvases a script makes before its recording session has a device. Web binds
// the replay worker's device at the first frame, after scripts init, so the
// canvas has to be minted from the session alone.

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "utils/no_op_factory.hpp"

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "common/render_context_null.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/render_canvas.hpp"

using namespace rive;

namespace
{
// Leaves init's `results` results on the stack.
void callInit(ScriptingTest& vm, int results)
{
  ScriptedObjectTest scriptedObjectTest;
  lua_State* L = vm.state();
  lua_getglobal(L, "init");
  lua_pushvalue(L, -2);
  lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
  if (lua_pcall(L, 2, results, 0) != LUA_OK)
  {
    FAIL(lua_tostring(L, -1));
  }
}

bool runInit(ScriptingTest& vm)
{
  callInit(vm, 1);
  return lua_toboolean(vm.state(), -1);
}

// Backs canvases on a GPU free context and records whose content it opened.
class BackingSink : public cmd::DeferredFrameSink
{
public:
  std::vector<gpu::RenderCanvas*> contentOpened;

  Factory* factory() override { return m_rc.get(); }
  ore::Context* oreContext() override { return nullptr; }
  gpu::RenderContext* renderContext() override { return m_rc.get(); }
  Renderer* beginScreenFrame(uint64_t) override { return nullptr; }
  Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
  {
    contentOpened.push_back(canvas);
    return nullptr;
  }

private:
  std::unique_ptr<gpu::RenderContext> m_rc = RenderContextNULL::MakeContext();
};
} // namespace

TEST_CASE(
    "a 2d canvas made before the device binds is backed at replay",
    "[scripting][deferred]")
{
  cmd::DeferredSession session(ore::ReplayCaps{});
  ScriptingTest vm(
      R"(
function init(self, context)
  local canvas = context:canvas({ width = 8, height = 8 })
  local resized = context:canvas()
  resized:resize(16, 4)
  assert(canvas.width == 8 and resized.width == 16, 'size')
  assert(canvas.image:view() ~= nil and resized.image:view() ~= nil, 'view')
  canvas:beginFrame()
  canvas:endFrame()
  resized:beginFrame()
  resized:endFrame()
  return canvas, resized
end
)",
      1,
      false,
      {},
      true,
      &session);
  callInit(vm, 2);
  lua_State* L = vm.state();
  auto* madeHandle = lua_torive<ScriptedCanvas>(L, -2);
  rcp<gpu::RenderCanvas> made = madeHandle->canvas;
  rcp<gpu::RenderCanvas> resized = lua_torive<ScriptedCanvas>(L, -1)->canvas;
  REQUIRE(made != nullptr);
  REQUIRE(resized != nullptr);
  CHECK(!made->isBacked());
  CHECK(!resized->isBacked());
  lua_getref(L, madeHandle->m_imageRef);
  CHECK(lua_torive<ScriptedImage>(L, -1)->sourceCanvas == made);
  lua_pop(L, 1);

  BackingSink sink;
  cmd::DeferredReplayer replayer;
  replayer.replayFrame(cmd::snapshotFrame(session), sink);

  CHECK(made->isBacked());
  CHECK(resized->isBacked());
  CHECK(resized->width() == 16);
  CHECK(
      sink.contentOpened ==
      std::vector<gpu::RenderCanvas*>{made.get(), resized.get()});
}

TEST_CASE(
    "an immediate 2d canvas still needs a render context",
    "[scripting][deferred]")
{
  NoOpFactory factory;
  ScriptingTest vm(
      R"(
function init(self, context)
  local ok, err = pcall(function()
    context:canvas({ width = 4, height = 4 })
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

#ifdef WITH_RIVE_TOOLS
TEST_CASE(
    "tooling warns once when a script samples a canvas mid frame",
    "[scripting][deferred]")
{
  cmd::DeferredSession session(ore::ReplayCaps{});
  ScriptingTest vm(
      R"(
function init(self, context)
  local a = context:canvas({ width = 8, height = 8 })
  local b = context:canvas({ width = 8, height = 8 })
  local sampler = ImageSampler('clamp', 'clamp', 'nearest')
  -- Quiet: a is closed, a draws itself, and a ends before b.
  local rb = b:beginFrame()
  rb:drawImage(a.image, sampler, 'srcOver', 1)
  b:endFrame()
  local ra = a:beginFrame()
  ra:drawImage(a.image, sampler, 'srcOver', 1)
  rb = b:beginFrame()
  a:endFrame()
  rb:drawImage(a.image, sampler, 'srcOver', 1)
  -- Warns once: b samples a while a is open.
  a:beginFrame()
  rb:drawImage(a.image, sampler, 'srcOver', 1)
  rb:drawImage(a.image, sampler, 'srcOver', 1)
  a:endFrame()
  b:endFrame()
  return true
end
)",
      1,
      false,
      {},
      true,
      &session);
  CHECK(runInit(vm));
  CHECK(vm.errors == std::vector<std::string>{cmd::kSampledOpenCanvasWarning});
}
#endif
#endif
