#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_factory.hpp"

using namespace rive;

namespace
{
class StubCanvasHost : public cmd::DeferredCanvasHost
{
public:
    Renderer* beginCanvasContent(gpu::RenderCanvas*, uint32_t) override
    {
        return nullptr;
    }
    void endCanvasContent(gpu::RenderCanvas*) override {}
};

// Import factory shaped like an FFI deferred session: a device is already
// bound and canvas work must record through the host.
class BoundSessionFactory : public NoOpFactory
{
public:
    StubCanvasHost host;
    Factory* renderContext() override { return this; }
    cmd::DeferredCanvasHost* deferredCanvasHost() override { return &host; }
};

// Import factory shaped like a web deferred session: no device yet, but the
// recording host exists from the start.
class UnboundSessionFactory : public NoOpFactory
{
public:
    StubCanvasHost host;
    cmd::DeferredCanvasHost* deferredCanvasHost() override { return &host; }
};
} // namespace

TEST_CASE("import routing wires the canvas host when the factory has a device",
          "[scripting]")
{
    BoundSessionFactory factory;
    auto file = ReadRiveFile("assets/script_advance_test.riv", &factory);
    auto* context = file->scriptingVM()->context();
    REQUIRE(context != nullptr);
    // renderContext() reads through a factory fallback, so the router must
    // not mistake the factory's own device for a caller-chosen one and skip
    // the host, which has no fallback of its own.
    CHECK(context->deferredCanvasHost() == &factory.host);
    CHECK(context->renderContext() == &factory);
    CHECK_FALSE(context->renderContextIsLateBound());
}

TEST_CASE("import routing wires the canvas host before any device exists",
          "[scripting]")
{
    UnboundSessionFactory factory;
    auto file = ReadRiveFile("assets/script_advance_test.riv", &factory);
    auto* context = file->scriptingVM()->context();
    REQUIRE(context != nullptr);
    CHECK(context->deferredCanvasHost() == &factory.host);
    // No device: stays late bound so canvas backings defer to whoever binds.
    CHECK(context->renderContextIsLateBound());
}

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
TEST_CASE("sized canvas construction goes pending until a device binds",
          "[scripting]")
{
    ScriptingTest vm(R"(
function init(self, context)
  local gpu = context:gpuCanvas({ width = 4, height = 4 })
  local c2d = context:canvas({ width = 4, height = 4 })
  return gpu ~= nil and c2d ~= nil
end
)");
    StubCanvasHost host;
    vm.vm()->context()->setDeferredCanvasHost(&host);

    ScriptedObjectTest scriptedObjectTest;
    lua_State* L = vm.state();
    lua_getglobal(L, "init");
    lua_pushvalue(L, -2);
    lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
    CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(L, -1));
}

TEST_CASE("sized canvas construction still refuses a deviceless factory",
          "[scripting]")
{
    ScriptingTest vm(R"(
function init(self, context)
  return context:gpuCanvas({ width = 4, height = 4 })
end
)");
    // No canvas host: this factory will never have a device, so pending
    // would be a silent forever-hang and the refusal must stay.
    ScriptedObjectTest scriptedObjectTest;
    lua_State* L = vm.state();
    lua_getglobal(L, "init");
    lua_pushvalue(L, -2);
    lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
    CHECK(lua_pcall(L, 2, 1, 0) != LUA_OK);
}
#endif
