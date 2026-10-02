#ifdef WITH_RIVE_SCRIPTNET

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive_file_reader.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/async/work_pool.hpp"
#include "rive/bindable_artboard.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/scriptnet/net.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "utils/serializing_factory.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace rive;

namespace
{
constexpr const char* kHostAsset = "assets/bindable_artboard_nesty.riv";
constexpr const char* kChildAsset = "assets/bindable_artboard_child.riv";

void pushBytes(lua_State* L, const std::vector<uint8_t>& bytes)
{
    void* data = lua_newbuffer(L, bytes.size());
    if (!bytes.empty())
    {
        memcpy(data, bytes.data(), bytes.size());
    }
}

// The default artboard of kHostAsset, bound to a fresh instance of its view
// model, whose `someArtboard` property is what the tests assign to.
struct Host
{
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> stateMachine;
    rcp<ViewModelInstance> viewModel;

    explicit Host(Factory* factory) :
        file(ReadRiveFile(kHostAsset, factory)),
        artboard(file->artboardDefault()),
        stateMachine(artboard->stateMachineAt(0)),
        viewModel(file->createViewModelInstance(artboard.get()))
    {}

    ViewModelInstanceArtboard* property()
    {
        return viewModel->propertyValue("someArtboard")
            ->as<ViewModelInstanceArtboard>();
    }

    // The nested artboard showing the property's bindable, if one is mounted.
    NestedArtboard* mounted()
    {
        auto bindable = property()->asset();
        if (bindable == nullptr)
        {
            return nullptr;
        }
        for (auto object : artboard->objects())
        {
            if (object == nullptr || !object->is<NestedArtboard>())
            {
                continue;
            }
            auto nested = object->as<NestedArtboard>();
            auto instance = nested->artboardInstance();
            if (instance != nullptr &&
                instance->artboardSource() ==
                    bindable->artboard()->artboardSource())
            {
                return nested;
            }
        }
        return nullptr;
    }

    // Whether any nested artboard shows an artboard named `name`.
    bool shows(const char* name)
    {
        for (auto object : artboard->objects())
        {
            if (object != nullptr && object->is<NestedArtboard>())
            {
                auto instance =
                    object->as<NestedArtboard>()->artboardInstance();
                if (instance != nullptr && instance->name() == name)
                {
                    return true;
                }
            }
        }
        return false;
    }
};

// Calls global `name` with a live Context, then `args` more values the
// caller pushed, and returns the call status. The error, if any, is left for
// the caller to read and pop.
int callWithContext(lua_State* L,
                    ScriptedObject* object,
                    const char* name,
                    int args)
{
    lua_getglobal(L, name);
    lua_insert(L, -(args + 1));
    lua_newrive<ScriptedContext>(L, object);
    lua_insert(L, -(args + 1));
    return lua_pcall(L, args + 1, 1, 0);
}

std::string popString(lua_State* L)
{
    const char* value = lua_tostring(L, -1);
    std::string result = value != nullptr ? value : "<nil>";
    lua_pop(L, 1);
    return result;
}

// Loads and keeps a decoded file in the global `file`, and exposes the
// operations the tests drive from C++.
constexpr const char* kScript = R"(
file = nil

function decode(context: Context, bytes: buffer)
    local status = "unsettled"
    context:decodeFile(bytes):andThen(function(decoded)
        file = decoded
        status = "ok"
    end, function(reason)
        status = reason
    end)
    return status
end

function names()
    return table.concat(file.artboardNames, ",")
end

function assign(host, name)
    local bindable = if name == "" then file:bindableArtboard()
        else file:bindableArtboard(name)
    host:getArtboard("someArtboard").value = bindable
    return if bindable == nil then "nil" else bindable.name
end

function clear(host)
    host:getArtboard("someArtboard").value = nil
    return nil
end

function forget()
    file = nil
    return nil
end
)";

struct ScriptHost
{
    ScriptingTest test;
    ScriptedObjectTest object;

    explicit ScriptHost(Factory* factory) :
        test(kScript, 1, false, {}, true, factory)
    {}

    lua_State* state() { return test.state(); }

    std::string decode(const std::vector<uint8_t>& bytes)
    {
        lua_State* L = state();
        pushBytes(L, bytes);
        REQUIRE(callWithContext(L, &object, "decode", 1) == LUA_OK);
        return popString(L);
    }

    std::string call(const char* name, ViewModelInstance* host, const char* arg)
    {
        lua_State* L = state();
        lua_getglobal(L, name);
        int args = 0;
        if (host != nullptr)
        {
            lua_newrive<ScriptedViewModel>(L,
                                           L,
                                           ref_rcp(host->viewModel()),
                                           ref_rcp(host));
            args++;
        }
        if (arg != nullptr)
        {
            lua_pushstring(L, arg);
            args++;
        }
        if (lua_pcall(L, args, 1, 0) != LUA_OK)
        {
            std::string error = popString(L);
            FAIL(error);
        }
        return popString(L);
    }

    void collect() { lua_gc(state(), LUA_GCCOLLECT, 0); }
};
} // namespace

TEST_CASE("context:decodeFile resolves with the decoded file",
          "[scripting][decode_file]")
{
    SerializingFactory factory;
    ScriptHost script(&factory);
    CHECK(script.decode(ReadFile(kChildAsset)) == "ok");

    auto expected = ReadRiveFile(kChildAsset, &factory);
    std::string names;
    for (size_t i = 0; i < expected->artboardCount(); i++)
    {
        names += (i == 0 ? "" : ",") + expected->artboardNameAt(i);
    }
    CHECK(script.call("names", nullptr, nullptr) == names);
}

TEST_CASE("context:decodeFile rejects bytes that are not a Rive file",
          "[scripting][decode_file]")
{
    SerializingFactory factory;
    ScriptHost script(&factory);
    CHECK(script.decode({1, 2, 3, 4, 5, 6, 7, 8}) ==
          "malformed: not a Rive file");

    // Past the header, with the body cut off.
    auto bytes = ReadFile(kChildAsset);
    bytes.resize(bytes.size() / 2);
    CHECK(script.decode(bytes) == "malformed: not a Rive file");
}

TEST_CASE("context:decodeFile raises on arguments that are not bytes",
          "[scripting][decode_file]")
{
    SerializingFactory factory;
    ScriptHost script(&factory);
    lua_State* L = script.state();

    pushBytes(L, {});
    REQUIRE(callWithContext(L, &script.object, "decode", 1) == LUA_ERRRUN);
    CHECK(popString(L).find("decodeFile: empty buffer") != std::string::npos);

    lua_pushstring(L, "RIVE");
    REQUIRE(callWithContext(L, &script.object, "decode", 1) == LUA_ERRRUN);
    CHECK(popString(L).find("buffer") != std::string::npos);
}

TEST_CASE("a decoded artboard assigned to a view model property is mounted",
          "[scripting][decode_file]")
{
    SerializingFactory factory;
    Host host(&factory);
    ScriptHost script(&factory);
    REQUIRE(script.decode(ReadFile(kChildAsset)) == "ok");

    host.stateMachine->bindViewModelInstance(host.viewModel);
    CHECK(script.call("assign", host.viewModel.get(), "Artboard") ==
          "Artboard");
    REQUIRE(host.property()->asset() != nullptr);
    CHECK(host.property()->asset()->artboard()->name() == "Artboard");

    host.stateMachine->advanceAndApply(0.016f);
    auto nested = host.mounted();
    REQUIRE(nested != nullptr);

    // Bound to an instance of the decoded file's own view model, not the
    // host's.
    auto bound = host.property()->boundViewModelInstance();
    REQUIRE(bound != nullptr);
    auto child = ReadRiveFile(kChildAsset, &factory);
    auto childArtboard = child->artboard("Artboard");
    REQUIRE(childArtboard != nullptr);
    CHECK(bound->viewModel()->name() ==
          child->viewModel((size_t)childArtboard->viewModelId())->name());
    REQUIRE(nested->artboardInstance()->dataContext() != nullptr);
    CHECK(nested->artboardInstance()->dataContext()->mainViewModelInstance() ==
          bound);

    // The default artboard is the one File::artboardDefault picks.
    CHECK(script.call("assign", host.viewModel.get(), "") ==
          child->artboardDefault()->name());
    // An unknown name is nil, and assigning nil clears the property.
    CHECK(script.call("assign", host.viewModel.get(), "Nope") == "nil");
    CHECK(host.property()->asset() == nullptr);
    CHECK(host.property()->boundViewModelInstance() == nullptr);
}

TEST_CASE("a script-assigned bindable renders like the embedder API",
          "[scripting][decode_file]")
{
    auto render = [](Host& host, SerializingFactory& factory) {
        factory.frameSize((uint32_t)host.artboard->width(),
                          (uint32_t)host.artboard->height());
        auto renderer = factory.makeRenderer();
        host.stateMachine->bindViewModelInstance(host.viewModel);
        host.stateMachine->advanceAndApply(0.016f);
        host.artboard->draw(renderer.get());
        factory.addFrame();
        host.stateMachine->pointerDown(Vec2D(250, 250));
        host.stateMachine->pointerUp(Vec2D(250, 250));
        host.stateMachine->advanceAndApply(0.016f);
        host.artboard->draw(renderer.get());
        for (int i = 0; i < 5; i++)
        {
            factory.addFrame();
            host.stateMachine->advanceAndApply(0.1f);
            host.artboard->draw(renderer.get());
        }
        factory.addFrame();
    };

    // The embedder path, with the same view model instance the script path
    // binds.
    SerializingFactory embedderFactory;
    Host embedderHost(&embedderFactory);
    auto child = ReadRiveFile(kChildAsset, &embedderFactory);
    {
        auto bindable = child->bindableArtboardNamed("Artboard");
        auto bound =
            child->createDefaultViewModelInstance(bindable->artboard());
        embedderHost.property()->boundViewModelInstance(bound);
        embedderHost.property()->asset(bindable);
    }
    render(embedderHost, embedderFactory);

    SerializingFactory scriptFactory;
    Host scriptHost(&scriptFactory);
    ScriptHost script(&scriptFactory);
    REQUIRE(script.decode(ReadFile(kChildAsset)) == "ok");
    REQUIRE(script.call("assign", scriptHost.viewModel.get(), "Artboard") ==
            "Artboard");
    render(scriptHost, scriptFactory);

    auto expected = embedderFactory.bytes();
    auto actual = scriptFactory.bytes();
    REQUIRE(expected.size() > 0);
    CHECK(std::vector<uint8_t>(actual.begin(), actual.end()) ==
          std::vector<uint8_t>(expected.begin(), expected.end()));
}

// Run under ASan: the decoded file has no owner but the property (and the
// nested artboard that mounted its clone), so dropping or replacing the
// bindable must not free definitions the mounted clone still uses.
TEST_CASE("a mounted bindable keeps its decoded file alive",
          "[scripting][decode_file]")
{
    SerializingFactory factory;
    Host host(&factory);
    ScriptHost script(&factory);
    host.stateMachine->bindViewModelInstance(host.viewModel);
    auto renderer = factory.makeRenderer();

    REQUIRE(script.decode(ReadFile(kChildAsset)) == "ok");
    REQUIRE(script.call("assign", host.viewModel.get(), "Artboard") ==
            "Artboard");
    script.call("forget", nullptr, nullptr);
    script.collect();
    for (int i = 0; i < 3; i++)
    {
        host.stateMachine->advanceAndApply(0.1f);
        host.artboard->draw(renderer.get());
    }
    REQUIRE(host.mounted() != nullptr);

    // Replace it with a bindable from a second decode, dropped the same way:
    // the first file is now owned by the mounted clone alone until the swap.
    REQUIRE(script.decode(ReadFile(kChildAsset)) == "ok");
    REQUIRE(script.call("assign", host.viewModel.get(), "Artboard") ==
            "Artboard");
    script.call("forget", nullptr, nullptr);
    script.collect();
    host.artboard->draw(renderer.get());
    for (int i = 0; i < 3; i++)
    {
        host.stateMachine->advanceAndApply(0.1f);
        host.artboard->draw(renderer.get());
    }
    REQUIRE(host.mounted() != nullptr);

    // And clear it.
    script.call("clear", host.viewModel.get(), nullptr);
    script.collect();
    host.artboard->draw(renderer.get());
    for (int i = 0; i < 3; i++)
    {
        host.stateMachine->advanceAndApply(0.1f);
        host.artboard->draw(renderer.get());
    }
    CHECK_FALSE(host.shows("Artboard"));
}

// A stateful host binds its own instances to what it shows. Those are of the
// host file's view models, so an artboard from another file has to keep the
// instance the script bound with it.
TEST_CASE("a stateful host keeps a decoded artboard's own view model",
          "[scripting][decode_file]")
{
    // Built by rive-cli from a Main artboard whose NestedArtboard "Slot"
    // (isStateful, with a ViewModelInstance child of OwnVM) is bound to the
    // Host view model's `someArtboard`, which defaults to the component
    // artboard "Own" (view model OwnVM).
    SerializingFactory factory;
    auto file = ReadRiveFile("assets/stateful_bindable_host.riv", &factory);
    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    auto viewModel = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModel != nullptr);
    stateMachine->bindViewModelInstance(viewModel);
    stateMachine->advanceAndApply(0.016f);

    NestedArtboard* slot = nullptr;
    for (auto object : artboard->objects())
    {
        if (object != nullptr && object->is<NestedArtboard>())
        {
            slot = object->as<NestedArtboard>();
        }
    }
    REQUIRE(slot != nullptr);
    REQUIRE(slot->isStateful());
    REQUIRE(slot->artboardInstance() != nullptr);
    CHECK(slot->artboardInstance()->name() == "Own");
    ViewModelInstance* statefulInstance = nullptr;
    for (auto child : slot->children())
    {
        if (child->is<ViewModelInstance>())
        {
            statefulInstance = child->as<ViewModelInstance>();
        }
    }
    REQUIRE(statefulInstance != nullptr);
    auto boundTo = [&]() {
        auto dataContext = slot->artboardInstance()->dataContext();
        return dataContext != nullptr
                   ? dataContext->mainViewModelInstance().get()
                   : nullptr;
    };
    CHECK(boundTo() == statefulInstance);

    ScriptHost script(&factory);
    REQUIRE(script.decode(ReadFile(kChildAsset)) == "ok");
    REQUIRE(script.call("assign", viewModel.get(), "Artboard") == "Artboard");
    for (int i = 0; i < 3; i++)
    {
        stateMachine->advanceAndApply(0.016f);
    }
    auto property = viewModel->propertyValue("someArtboard")
                        ->as<ViewModelInstanceArtboard>();
    REQUIRE(slot->artboardInstance() != nullptr);
    CHECK(slot->artboardInstance()->name() == "Artboard");
    REQUIRE(property->boundViewModelInstance() != nullptr);
    CHECK(boundTo() == property->boundViewModelInstance().get());

    // Back to the host's own artboard: the stateful instance again.
    property->propertyValue(1);
    for (int i = 0; i < 3; i++)
    {
        stateMachine->advanceAndApply(0.016f);
    }
    REQUIRE(slot->artboardInstance() != nullptr);
    CHECK(slot->artboardInstance()->name() == "Own");
    CHECK(boundTo() == statefulInstance);
}

TEST_CASE("replacing one bindable with another notifies listeners",
          "[scripting][decode_file]")
{
    SerializingFactory factory;
    Host host(&factory);
    ScriptingTest test(R"(
file = nil
changes = 0

function decode(context: Context, bytes: buffer)
    context:decodeFile(bytes):andThen(function(decoded)
        file = decoded
    end)
    return nil
end

function listen(host)
    host:getArtboard("someArtboard"):addListener(function()
        changes += 1
    end)
    return nil
end

function assign(host)
    host:getArtboard("someArtboard").value = file:bindableArtboard("Artboard")
    return changes
end
)",
                       1,
                       false,
                       {},
                       true,
                       &factory);
    lua_State* L = test.state();
    ScriptedObjectTest object;
    pushBytes(L, ReadFile(kChildAsset));
    REQUIRE(callWithContext(L, &object, "decode", 1) == LUA_OK);
    lua_pop(L, 1);

    auto callWithHost = [&](const char* name) {
        lua_getglobal(L, name);
        lua_newrive<ScriptedViewModel>(L,
                                       L,
                                       ref_rcp(host.viewModel->viewModel()),
                                       host.viewModel);
        REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
        int result = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return result;
    };
    callWithHost("listen");
    CHECK(callWithHost("assign") == 1);
    // A second bindable: the property already held one, so only asset()'s
    // own notification reports the swap.
    CHECK(callWithHost("assign") == 2);
}

TEST_CASE("reassigning the same bindable does not remount it",
          "[scripting][decode_file]")
{
    SerializingFactory factory;
    Host host(&factory);
    ScriptingTest test(R"(
file = nil
bindable = nil

function decode(context: Context, bytes: buffer)
    context:decodeFile(bytes):andThen(function(decoded)
        file = decoded
    end)
    return nil
end

function fresh(host)
    bindable = file:bindableArtboard("Artboard")
    host:getArtboard("someArtboard").value = bindable
    return nil
end

function same(host)
    host:getArtboard("someArtboard").value = bindable
    return nil
end
)",
                       1,
                       false,
                       {},
                       true,
                       &factory);
    lua_State* L = test.state();
    ScriptedObjectTest object;
    pushBytes(L, ReadFile(kChildAsset));
    REQUIRE(callWithContext(L, &object, "decode", 1) == LUA_OK);
    lua_pop(L, 1);
    host.stateMachine->bindViewModelInstance(host.viewModel);

    auto callWithHost = [&](const char* name) {
        lua_getglobal(L, name);
        lua_newrive<ScriptedViewModel>(L,
                                       L,
                                       ref_rcp(host.viewModel->viewModel()),
                                       host.viewModel);
        REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
        lua_pop(L, 1);
        host.stateMachine->advanceAndApply(0.016f);
        auto nested = host.mounted();
        REQUIRE(nested != nullptr);
        return nested->artboardInstance();
    };
    auto mounted = callWithHost("fresh");
    CHECK(callWithHost("same") == mounted);
    CHECK(callWithHost("same") == mounted);
    CHECK(callWithHost("fresh") != mounted);
}

TEST_CASE("decoded files run only signed scripts, in their own VM",
          "[scripting][decode_file]")
{
    auto scriptAssets = [](File* file) {
        std::vector<ScriptAsset*> scripts;
        for (auto asset : file->assets())
        {
            if (asset->is<ScriptAsset>())
            {
                scripts.push_back(asset->as<ScriptAsset>());
            }
        }
        return scripts;
    };

    // Unsigned: a tools build runs these for an embedder, but not for a file
    // a script decoded.
    constexpr const char* kUnsignedAsset = "assets/library_scope_edge_test.riv";
    auto unsignedBytes = ReadFile(kUnsignedAsset);
    {
        auto embedded = ReadRiveFile(kUnsignedAsset);
        auto scripts = scriptAssets(embedded.get());
        REQUIRE(!scripts.empty());
        for (auto script : scripts)
        {
            REQUIRE_FALSE(script->verified());
        }
        CHECK(embedded->scriptingVM() != nullptr);
    }
    ImportResult result;
    auto decoded = File::import(unsignedBytes,
                                &gNoOpFactory,
                                &result,
                                nullptr,
                                nullptr,
                                /*requireSignedScripts*/ true);
    REQUIRE(decoded != nullptr);
    CHECK(decoded->scriptingVM() == nullptr);
    for (auto script : scriptAssets(decoded.get()))
    {
        CHECK(script->generatorFunctionRef() == 0);
    }

    // Signed with the production key: registered as usual.
    auto signedDecoded = File::import(ReadFile("assets/joel_signed.riv"),
                                      &gNoOpFactory,
                                      &result,
                                      nullptr,
                                      nullptr,
                                      /*requireSignedScripts*/ true);
    REQUIRE(signedDecoded != nullptr);
    CHECK(signedDecoded->scriptingVM() != nullptr);
    bool registered = false;
    for (auto script : scriptAssets(signedDecoded.get()))
    {
        CHECK(script->verified());
        registered = registered || script->generatorFunctionRef() != 0;
    }
    CHECK(registered);
}

namespace
{
class BytesProvider : public scriptnet::Provider
{
public:
    std::vector<scriptnet::RequestId> starts;
    void start(scriptnet::RequestId id, const scriptnet::HttpRequest&) override
    {
        starts.push_back(id);
    }
    void abort(scriptnet::RequestId) override {}
};
} // namespace

TEST_CASE("an async body fetches, decodes and binds an artboard",
          "[scripting][decode_file]")
{
    auto provider = make_rcp<BytesProvider>();
    scriptnet::setProvider(provider);
    scriptnet::setLimits(scriptnet::NetLimits());
    {
        SerializingFactory factory;
        Host host(&factory);
        ScriptingTest test(R"(
state = { step = "started" }

function load(context: Context, host)
    local character = host:getArtboard("someArtboard")
    async(function()
        local ok, response = await(fetch("https://example.com/child.riv"))
        if not ok then
            state.step = `fetch failed: {response}`
            return nil
        end
        local _, bytes = await(response:arrayBuffer())
        local loaded, file = await(context:decodeFile(bytes))
        if not loaded then
            state.step = `decode failed: {file}`
            return nil
        end
        -- Resolved inside the async body on purpose: view model access has
        -- to work from a coroutine.
        host:getArtboard("someArtboard").value = file:bindableArtboard("Artboard")
        state.step = if character.value ~= nil then character.value.name
            else "unbound"
        return nil
    end)
    return nil
end
)",
                           1,
                           false,
                           {},
                           true,
                           &factory);
        lua_State* L = test.state();
        ScriptedObjectTest object;
        lua_newrive<ScriptedViewModel>(L,
                                       L,
                                       ref_rcp(host.viewModel->viewModel()),
                                       host.viewModel);
        REQUIRE(callWithContext(L, &object, "load", 1) == LUA_OK);
        lua_pop(L, 1);
        REQUIRE(provider->starts.size() == 1);

        scriptnet::HttpResponse response;
        response.status = 200;
        response.url = "https://example.com/child.riv";
        response.body = ReadFile(kChildAsset);
        scriptnet::complete(provider->starts[0], std::move(response));
        rive_pollAsyncWork();

        lua_getglobal(L, "state");
        lua_getfield(L, -1, "step");
        CHECK(popString(L) == "Artboard");
        lua_pop(L, 1);

        host.stateMachine->bindViewModelInstance(host.viewModel);
        host.stateMachine->advanceAndApply(0.016f);
        CHECK(host.mounted() != nullptr);
    }
    scriptnet::setProvider(nullptr);
}

#endif
