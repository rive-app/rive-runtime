#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/command_queue.hpp"
#include "rive/command_server.hpp"
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

// Shared shape of the deferred session doubles. The ore pointer is a
// per-instance stand-in, only ever compared.
class SessionFactory : public NoOpFactory
{
public:
    StubCanvasHost host;
    cmd::DeferredCanvasHost* deferredCanvasHost() override { return &host; }
    ore::Context* ore() override
    {
        return reinterpret_cast<ore::Context*>(&host);
    }
};

// Import factory shaped like an FFI deferred session: a device is already
// bound and canvas work must record through the host.
class BoundSessionFactory : public SessionFactory
{
public:
    // Callers cast renderContext() to gpu::RenderContext*, so a session must
    // not answer with itself.
    NoOpFactory device;
    Factory* renderContext() override { return &device; }
};

// Import factory shaped like a web deferred session: no device yet, but the
// recording host exists from the start.
class UnboundSessionFactory : public SessionFactory
{};
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
    CHECK(context->oreContext() == static_cast<void*>(factory.ore()));
    CHECK(context->renderContext() == &factory.device);
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
    CHECK(context->oreContext() == static_cast<void*>(factory.ore()));
    // No device: stays late bound so canvas backings defer to whoever binds.
    CHECK(context->renderContextIsLateBound());
}

// The case File::makeScriptingVM cannot produce: a VM built against one
// factory, imported through another. Both Dart decode paths mint the
// recording session inside decode, after the caller has already built its VM.
TEST_CASE("a caller supplied VM routes through the import factory",
          "[scripting]")
{
    NoOpFactory constructionFactory;
    auto vm = make_rcp<ScriptingVM>(
        std::make_unique<CPPRuntimeScriptingContext>(&constructionFactory));

    BoundSessionFactory importFactory;
    std::vector<uint8_t> bytes = ReadFile("assets/script_advance_test.riv");
    ImportResult result;
    auto file = File::import(bytes, &importFactory, &result, nullptr, vm.get());
    REQUIRE(result == ImportResult::success);

    auto* context = vm->context();
    REQUIRE(context != nullptr);
    CHECK(context->factory() == &importFactory);
    CHECK(context->deferredCanvasHost() == &importFactory.host);
    CHECK(context->oreContext() == static_cast<void*>(importFactory.ore()));
    CHECK(context->renderContext() == &importFactory.device);
}

// The editor's shape: it builds its VM against the same session it imports
// through, so re-pointing must leave it exactly where it was.
TEST_CASE("a VM built against the import factory keeps its routing",
          "[scripting]")
{
    BoundSessionFactory factory;
    auto vm = make_rcp<ScriptingVM>(
        std::make_unique<CPPRuntimeScriptingContext>(&factory));

    std::vector<uint8_t> bytes = ReadFile("assets/script_advance_test.riv");
    ImportResult result;
    auto file = File::import(bytes, &factory, &result, nullptr, vm.get());
    REQUIRE(result == ImportResult::success);

    auto* context = vm->context();
    REQUIRE(context != nullptr);
    CHECK(context->factory() == &factory);
    CHECK(context->deferredCanvasHost() == &factory.host);
    CHECK(context->oreContext() == static_cast<void*>(factory.ore()));
}

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
TEST_CASE("sized canvas construction goes pending until a device binds",
          "[scripting]")
{
    // The factory answers deferredCanvasHost(), the same derivation a session
    // import gives a real host.
    UnboundSessionFactory factory;
    ScriptingTest vm(R"(
function init(self, context)
  local gpu = context:gpuCanvas({ width = 4, height = 4 })
  local c2d = context:canvas({ width = 4, height = 4 })
  return gpu ~= nil and c2d ~= nil
end
)",
                     1,
                     false,
                     {},
                     true,
                     &factory);

    ScriptedObjectTest scriptedObjectTest;
    lua_State* L = vm.state();
    lua_getglobal(L, "init");
    lua_pushvalue(L, -2);
    lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
    CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(L, -1));
}

// The reachable symptom of the mismatch: without routing the script is
// refused outright rather than recording for the device that binds later.
TEST_CASE("a caller supplied VM's sized canvas goes pending, not refused",
          "[scripting]")
{
    NoOpFactory constructionFactory;
    ScriptingTest vm(R"(
function init(self, context)
  local gpu = context:gpuCanvas({ width = 4, height = 4 })
  local c2d = context:canvas({ width = 4, height = 4 })
  return gpu ~= nil and c2d ~= nil
end
)",
                     1,
                     false,
                     {},
                     false,
                     &constructionFactory);

    UnboundSessionFactory importFactory;
    std::vector<uint8_t> bytes = ReadFile("assets/script_advance_test.riv");
    ImportResult result;
    auto file = File::import(bytes, &importFactory, &result, nullptr, vm.vm());
    REQUIRE(result == ImportResult::success);
    vm.execute();

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

// The command server builds the VM itself, so a host cannot route the file
// after import the way a direct File::import caller can.
TEST_CASE("a command server load routes the device off the import factory",
          "[scripting]")
{
    BoundSessionFactory factory;
    auto commandQueue = make_rcp<CommandQueue>();
    CommandServer server(commandQueue, &factory);

    FileHandle fileHandle =
        commandQueue->loadFile(ReadFile("assets/script_advance_test.riv"));

    ScriptingContext* context = nullptr;
    commandQueue->runOnce([&](CommandServer* s) {
        if (auto* file = s->getFile(fileHandle))
        {
            context = file->scriptingVM()->context();
        }
    });
    server.processCommands();

    REQUIRE(context != nullptr);
    CHECK(context->factory() == &factory);
    CHECK(context->deferredCanvasHost() == &factory.host);
    CHECK(context->oreContext() == static_cast<void*>(factory.ore()));
    // The device, never the session: scripts cast this to gpu::RenderContext*.
    CHECK(context->renderContext() == &factory.device);

    commandQueue->disconnect();
}
