#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "utils/no_op_factory.hpp"

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"

using namespace rive;

namespace
{
// Records into a real deferred Ore context, the one a host session hands
// scripts, with no device behind it.
class TargetFactory : public NoOpFactory
{
public:
    ore::cmd::DeferredOreContext oreContext{ore::ReplayCaps{}};
    ore::Context* ore() override { return &oreContext; }
};

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

TEST_CASE("context:gpuTarget is nil without a recording context", "[scripting]")
{
    NoOpFactory factory;
    ScriptingTest vm(R"(
function init(self, context)
  return context:gpuTarget() == nil
end
)",
                     1,
                     false,
                     {},
                     true,
                     &factory);
    CHECK(runInit(vm));
}

TEST_CASE("context:gpuTarget is there in init before a target is declared",
          "[scripting]")
{
    TargetFactory factory;
    ScriptingTest vm(R"(
function init(self, context)
  local target = context:gpuTarget()
  return target ~= nil and target.view == nil and target.width == 0
end
)",
                     1,
                     false,
                     {},
                     true,
                     &factory);
    CHECK(runInit(vm));
}

TEST_CASE("context:gpuTarget is nil where the host can never expose one",
          "[scripting]")
{
    TargetFactory factory;
    factory.oreContext.setExposesTarget(false);
    ScriptingTest vm(R"(
function init(self, context)
  return context:gpuTarget() == nil
end
)",
                     1,
                     false,
                     {},
                     true,
                     &factory);
    CHECK(runInit(vm));
}

TEST_CASE("a hidden target still validates the pass it drops", "[scripting]")
{
    TargetFactory factory;
    ScriptingTest vm(R"(
function init(self, context)
  local target = context:gpuTarget()
  target:beginRenderPass({ color = {{ storeOp = 'store' }} }):finish()
  local ok = pcall(function()
    target:beginRenderPass({ color = {{ loadOp = 'clear' }} })
  end)
  return not ok
end
)",
                     1,
                     false,
                     {},
                     true,
                     &factory);
    CHECK(runInit(vm));
    CHECK_FALSE(factory.oreContext.targetDrawn());
}

TEST_CASE("a script reads the target and draws into it", "[scripting]")
{
    TargetFactory factory;
    factory.oreContext.setTarget({64, 32, ore::TextureFormat::bgra8unorm, 4});
    ScriptingTest vm(R"(
function init(self, context)
  local target = context:gpuTarget()
  assert(target.width == 64 and target.height == 32)
  assert(target.format == 'bgra8unorm' and target.sampleCount == 4)
  assert(target.view ~= nil)
  local pass = target:beginRenderPass({
    color = {{ loadOp = 'clear', storeOp = 'store' }},
  })
  pass:finish()
  return true
end
)",
                     1,
                     false,
                     {},
                     true,
                     &factory);
    CHECK(runInit(vm));
    CHECK(factory.oreContext.targetDrawn());
}

TEST_CASE("a pass on a frame without a target drops its commands",
          "[scripting]")
{
    TargetFactory factory;
    factory.oreContext.setTarget({64, 64});
    ScriptingTest vm(R"(
local target
function init(self, context)
  target = context:gpuTarget()
  return target ~= nil
end
function drawLater()
  local pass = target:beginRenderPass({ color = {{ storeOp = 'store' }} })
  pass:setViewport(0, 0, 1, 1)
  pass:finish()
  return target.width == 0 and target.view == nil
end
)",
                     1,
                     false,
                     {},
                     true,
                     &factory);
    REQUIRE(runInit(vm));

    factory.oreContext.resetFrame();
    factory.oreContext.setTarget({});
    lua_State* L = vm.state();
    lua_getglobal(L, "drawLater");
    REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(L, -1));
    CHECK_FALSE(factory.oreContext.targetDrawn());
}

TEST_CASE("a cached target view drops while hidden and errors once replaced",
          "[scripting]")
{
    TargetFactory factory;
    factory.oreContext.setTarget({64, 64});
    ScriptingTest vm(R"(
local target, view
function init(self, context)
  target = context:gpuTarget()
  view = target.view
  return view ~= nil
end
function drawCached()
  target:beginRenderPass({ color = {{ view = view, storeOp = 'store' }} })
    :finish()
end
)",
                     1,
                     false,
                     {},
                     true,
                     &factory);
    REQUIRE(runInit(vm));
    lua_State* L = vm.state();
    auto drawCached = [&]() {
        lua_getglobal(L, "drawCached");
        bool ok = lua_pcall(L, 0, 0, 0) == LUA_OK;
        if (!ok)
        {
            lua_pop(L, 1);
        }
        return ok;
    };

    // Hidden for a frame, the same target comes back with the same view.
    factory.oreContext.resetFrame();
    factory.oreContext.setTarget({});
    CHECK(drawCached());
    CHECK_FALSE(factory.oreContext.targetDrawn());

    factory.oreContext.resetFrame();
    factory.oreContext.setTarget({64, 64});
    CHECK(drawCached());
    CHECK(factory.oreContext.targetDrawn());

    // A resized target retires the view, which a pass now reports.
    factory.oreContext.resetFrame();
    factory.oreContext.setTarget({128, 64});
    CHECK_FALSE(drawCached());
}
#endif
