
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/view_model_type.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_factory.hpp"
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#endif
#include <string>

using namespace rive;

// Test helper: ScriptedObject with an associated ScriptAsset that references a
// File
class ScriptedObjectWithFile : public ScriptedObject
{
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        return true;
    }
    bool m_markedToUpdate = false;
    ScriptProtocol scriptProtocol() override { return ScriptProtocol::utility; }
    void markNeedsUpdate() override { m_markedToUpdate = true; }
    Component* component() override { return nullptr; }

    rcp<ScriptAsset> m_scriptAsset;

public:
    void setFileForScriptAsset(File* file)
    {
        m_scriptAsset = make_rcp<ScriptAsset>();
        m_scriptAsset->file(file);
        setAsset(m_scriptAsset);
    }
    uint32_t assetId() override
    {
        return m_scriptAsset ? m_scriptAsset->assetId() : 0;
    }
    bool needsUpdate() { return m_markedToUpdate; }
};

TEST_CASE("Scripted Context markNeedsUpdate works", "[scripting]")
{
    ScriptingTest vm(
        R"(

-- Called once when the script initializes.
function init(self: MyNode, context: Context): boolean
  context:markNeedsUpdate()
  return true
end

)");
    ScriptedObjectTest scriptedObjectTest;
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    {
        lua_getglobal(L, "init");
        lua_pushvalue(L, -2);
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        rive_lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
        CHECK(scriptedObjectTest.needsUpdate());
    }
}

TEST_CASE("Scripted Context errors when used after disposal", "[scripting]")
{
    ScriptingTest vm(
        R"(
function callMarkNeedsUpdate(context: Context)
  context:markNeedsUpdate()
end
)");
    ScriptedObjectTest scriptedObjectTest;
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    lua_getglobal(L, "callMarkNeedsUpdate");
    auto scriptedContext = lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
    scriptedContext->clearScriptedObject();
    int result = lua_pcall(L, 1, 0, 0);
    REQUIRE(result == LUA_ERRRUN);
    const char* error = lua_tostring(L, -1);
    REQUIRE(error != nullptr);
    CHECK(std::string(error).find(
              "context:markNeedsUpdate() called on a disposed context") !=
          std::string::npos);
    lua_pop(L, 1);
    CHECK(top == lua_gettop(L));
}

TEST_CASE("script has access to user created view models via Data", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/script_create_viewmodel_instance.riv", &silver);
    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    // Push element
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("newButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Push element at specific index
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("newAtButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Swap elements from indexes
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("swapButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Shift first element
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("shiftButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Pop last element
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("popButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Pop all elements and pop beyond empty list
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("popButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        trigger->trigger();
        trigger->trigger();
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Push 2 elements
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("newButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("script_create_viewmodel_instance"));
}

// Regression test for rive-ios#454: when a ScriptingVM is supplied to
// File::import (as the CommandServer does on iOS), the Lua `Data` global —
// which exposes view model constructors like Data.ProbeChipVM.new() — must
// still be initialized. Before the fix, initializeLuaData only ran inside
// makeScriptingVM(), which is skipped when an external VM is provided, so
// `Data` was nil in scripts.
TEST_CASE("Data global is initialized when a ScriptingVM is provided to import",
          "[scripting]")
{
    // Mirror CommandServer::processCommands: create the VM up front and pass
    // it into File::import.
    auto context =
        std::make_unique<rive::CPPRuntimeScriptingContext>(&gNoOpFactory);
    auto vm = rive::make_rcp<rive::ScriptingVM>(std::move(context));

    auto bytes = ReadFile("assets/data_global_repro.riv");
    rive::ImportResult result;
    auto file =
        rive::File::import(bytes, &gNoOpFactory, &result, nullptr, vm.get());
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);

    // The file adopts the VM we supplied...
    REQUIRE(file->scriptingVM() == vm.get());

    // ...and its `Data` global is a populated table (was nil before the fix),
    // exposing the file's view model as a constructor.
    lua_State* L = vm->state();
    REQUIRE(L != nullptr);
    lua_getglobal(L, "Data");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "ProbeChipVM");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "new");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 3);
}

TEST_CASE("script has access to the data bound view model", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/viewmodel_from_context.riv", &silver);
    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("viewmodel_from_context"));
}

TEST_CASE("script has access to the data root view model", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/scripting_root_viewmodel.riv", &silver);
    auto artboard = file->artboardNamed("parent");

    silver.frameSize(artboard->width(), artboard->height());
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("scripting_root_viewmodel"));
}

TEST_CASE("context:image returns image asset by name", "[scripting]")
{
    // Load a file with image assets using SerializingFactory which decodes
    // images
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/walle.riv", &silver);
    REQUIRE(file != nullptr);

    // Verify the file has image assets with render images
    auto assets = file->assets();
    bool foundImageWithRenderImage = false;
    for (const auto& asset : assets)
    {
        if (asset->is<ImageAsset>())
        {
            ImageAsset* imageAsset = asset->as<ImageAsset>();
            if (imageAsset->name() == "walle.jpg" &&
                imageAsset->renderImage() != nullptr)
            {
                foundImageWithRenderImage = true;
                break;
            }
        }
    }
    REQUIRE(foundImageWithRenderImage);

    // Create a ScriptedObject with a ScriptAsset that references this file
    ScriptedObjectWithFile scriptedObjectWithFile;
    scriptedObjectWithFile.setFileForScriptAsset(file.get());

    // Create a scripting VM and test context:image()
    ScriptingTest vm(
        R"(
local foundImage = nil
local imageWidth = 0
local imageHeight = 0

function testImage(context: Context)
  local img = context:image("walle.jpg")
  if img then
    foundImage = true
    imageWidth = img.width
    imageHeight = img.height
  else
    foundImage = false
  end
end

function getFoundImage(): boolean?
  return foundImage
end

function getImageWidth(): number
  return imageWidth
end

function getImageHeight(): number
  return imageHeight
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Call testImage with the context
    {
        lua_getglobal(L, "testImage");
        lua_newrive<ScriptedContext>(L, &scriptedObjectWithFile);
        int result = lua_pcall(L, 1, 0, 0);
        if (result != LUA_OK)
        {
            const char* error = lua_tostring(L, -1);
            fprintf(stderr, "Lua error: %s\n", error);
            lua_pop(L, 1);
        }
        CHECK(result == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Verify the image was found
    {
        lua_getglobal(L, "getFoundImage");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_toboolean(L, -1) == 1);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }

    // Verify dimensions (SerializingFactory provides real decoded images)
    {
        lua_getglobal(L, "getImageWidth");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) > 0);
        lua_pop(L, 1);

        lua_getglobal(L, "getImageHeight");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) > 0);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:image returns nil for non-existent image", "[scripting]")
{
    // Load a file with image assets
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/walle.riv", &silver);
    REQUIRE(file != nullptr);

    // Create a ScriptedObject with a ScriptAsset that references this file
    ScriptedObjectWithFile scriptedObjectWithFile;
    scriptedObjectWithFile.setFileForScriptAsset(file.get());

    // Create a scripting VM and test context:image() with non-existent name
    ScriptingTest vm(
        R"(
local foundImage = nil

function testNonExistentImage(context: Context)
  local img = context:image("this_image_does_not_exist")
  if img then
    foundImage = true
  else
    foundImage = false
  end
end

function getFoundImage(): boolean?
  return foundImage
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Call testNonExistentImage with the context
    {
        lua_getglobal(L, "testNonExistentImage");
        lua_newrive<ScriptedContext>(L, &scriptedObjectWithFile);
        int result = lua_pcall(L, 1, 0, 0);
        if (result != LUA_OK)
        {
            const char* error = lua_tostring(L, -1);
            fprintf(stderr, "Lua error: %s\n", error);
            lua_pop(L, 1);
        }
        CHECK(result == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Verify the image was NOT found (should be false, not nil)
    {
        lua_getglobal(L, "getFoundImage");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:image returns nil when no script asset", "[scripting]")
{
    // Create a ScriptedObject WITHOUT a file (default ScriptedObjectTest)
    ScriptedObjectTest scriptedObjectTest;

    // Create a scripting VM and test context:image()
    ScriptingTest vm(
        R"(
local foundImage = nil

function testNoFile(context: Context)
  local img = context:image("anyname")
  if img then
    foundImage = true
  else
    foundImage = false
  end
end

function getFoundImage(): boolean?
  return foundImage
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Call testNoFile with the context (which has no scriptAsset/file)
    {
        lua_getglobal(L, "testNoFile");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        int result = lua_pcall(L, 1, 0, 0);
        if (result != LUA_OK)
        {
            const char* error = lua_tostring(L, -1);
            fprintf(stderr, "Lua error: %s\n", error);
            lua_pop(L, 1);
        }
        CHECK(result == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Verify the image was NOT found
    {
        lua_getglobal(L, "getFoundImage");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("expose data context to scripts through context", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/scripted_data_context.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("scripted_data_context"));
}

TEST_CASE("Provide data context and view model instance to artboard",
          "[silver]")
{
    SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/viewmodel_instance_to_artboard.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("viewmodel_instance_to_artboard"));
}

TEST_CASE("context methods error on disposed context", "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
function testDisposed(context: Context)
  context:markNeedsUpdate()
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Create a context and dispose it before calling methods
    auto* ctx = lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
    int ctxIdx = lua_gettop(L);
    ctx->clearScriptedObject();
    CHECK(ctx->scriptedObject() == nullptr);

    // Calling a method on a disposed context should error
    {
        lua_getglobal(L, "testDisposed");
        lua_pushvalue(L, ctxIdx);
        int result = lua_pcall(L, 1, 0, 0);
        CHECK(result == LUA_ERRRUN);
        // Error message should mention "disposed context"
        const char* err = lua_tostring(L, -1);
        CHECK(err != nullptr);
        CHECK(std::string(err).find("disposed context") != std::string::npos);
        lua_pop(L, 1); // pop error
    }

    lua_pop(L, 1); // pop ctx
    CHECK(top == lua_gettop(L));
}

TEST_CASE("context:viewModel returns nil with no data context", "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
local result = "not_called"

function testViewModel(context: Context)
  local vm = context:viewModel()
  if vm == nil then
    result = "nil"
  else
    result = "found"
  end
end

function getResult(): string
  return result
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testViewModel");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getResult");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "nil");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:rootViewModel returns nil with no data context",
          "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
local result = "not_called"

function testRootViewModel(context: Context)
  local vm = context:rootViewModel()
  if vm == nil then
    result = "nil"
  else
    result = "found"
  end
end

function getResult(): string
  return result
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testRootViewModel");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getResult");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "nil");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:globalViewModel returns nil with no data context",
          "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
local result = "not_called"

function testGlobalViewModel(context: Context)
  local vm = context:globalViewModel("Anything")
  if vm == nil then
    result = "nil"
  else
    result = "found"
  end
end

function getResult(): string
  return result
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testGlobalViewModel");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getResult");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "nil");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:globalViewModelNames returns empty table with no file",
          "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
local count = -1

function testNames(context: Context)
  local names = context:globalViewModelNames()
  count = #names
end

function getCount(): number
  return count
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testNames");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 0);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:globalViewModel returns a bound global by name",
          "[scripting]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/global_variables_test.riv", &silver);
    REQUIRE(file != nullptr);

    // The fixture must declare at least one global view model.
    auto globalNames = file->globalViewModelNames();
    REQUIRE(!globalNames.empty());
    const std::string globalName = globalNames.front();

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    // Build a data context holding the artboard's main instance plus the global
    // bound into its slot (keyed by the global's file index), mirroring what
    // the runtime does when binding globals.
    auto mainInstance = file->createDefaultViewModelInstance(artboard.get());
    auto global =
        file->createDefaultViewModelInstance(file->viewModel(globalName));
    REQUIRE(global != nullptr);

    auto dataContext = make_rcp<DataContext>(mainInstance);
    dataContext->setViewModelInstanceForSlot(file->viewModelId(globalName),
                                             global);

    ScriptedObjectWithFile scriptedObjectWithFile;
    scriptedObjectWithFile.setFileForScriptAsset(file.get());
    scriptedObjectWithFile.dataContext(dataContext);

    ScriptingTest vm(
        R"(
local found = false
local nameCount = 0
local nameMatched = false

function testGlobal(context: Context, name: string)
  local vm = context:globalViewModel(name)
  found = vm ~= nil

  local names = context:globalViewModelNames()
  nameCount = #names
  for _, n in ipairs(names) do
    if n == name then
      nameMatched = true
    end
  end
end

function getFound(): boolean
  return found
end

function getNameCount(): number
  return nameCount
end

function getNameMatched(): boolean
  return nameMatched
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testGlobal");
        lua_newrive<ScriptedContext>(L, &scriptedObjectWithFile);
        lua_pushstring(L, globalName.c_str());
        int result = lua_pcall(L, 2, 0, 0);
        if (result != LUA_OK)
        {
            fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        CHECK(result == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getFound");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_toboolean(L, -1) == 1);
        lua_pop(L, 1);
    }

    {
        lua_getglobal(L, "getNameCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == (double)globalNames.size());
        lua_pop(L, 1);
    }

    {
        lua_getglobal(L, "getNameMatched");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_toboolean(L, -1) == 1);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:globalViewModel finds a global in a fully unslotted chain",
          "[scripting]")
{
    // The editor (and nested-artboard propagation inside it) builds data
    // contexts with the vector DataContext constructor, which assigns NO slot
    // keys anywhere in the chain. This reproduces that shape — a nested,
    // unslotted child whose parent is an unslotted root — and verifies the
    // identity-scan fallback in pushGlobalViewModel still resolves the global.
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/global_variables_test.riv", &silver);
    REQUIRE(file != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE(!globalNames.empty());
    const std::string globalName = globalNames.front();
    const uint32_t slotKey = file->viewModelId(globalName);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto global =
        file->createDefaultViewModelInstance(file->viewModel(globalName));
    REQUIRE(global != nullptr);

    // Root context: unslotted [main, global] (vector constructor), like the
    // editor's internalDataContextFromInstances.
    auto rootMain = file->createDefaultViewModelInstance(artboard.get());
    std::vector<rcp<ViewModelInstance>> rootInstances{rootMain, global};
    auto rootContext = make_rcp<DataContext>(std::move(rootInstances));

    // Nested child context: also unslotted, parented to the unslotted root.
    auto nestedMain = file->createDefaultViewModelInstance(artboard.get());
    std::vector<rcp<ViewModelInstance>> instances{nestedMain, global};
    auto dataContext = make_rcp<DataContext>(std::move(instances));
    dataContext->parent(rootContext);
    // Sanity: no slot keys exist anywhere, so slot lookup misses at every
    // level.
    REQUIRE(dataContext->instanceForSlot(slotKey) == nullptr);
    REQUIRE(rootContext->instanceForSlot(slotKey) == nullptr);

    ScriptedObjectWithFile scriptedObjectWithFile;
    scriptedObjectWithFile.setFileForScriptAsset(file.get());
    scriptedObjectWithFile.dataContext(dataContext);

    ScriptingTest vm(
        R"(
local found = false

function testGlobal(context: Context, name: string)
  found = context:globalViewModel(name) ~= nil
end

function getFound(): boolean
  return found
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testGlobal");
        lua_newrive<ScriptedContext>(L, &scriptedObjectWithFile);
        lua_pushstring(L, globalName.c_str());
        int result = lua_pcall(L, 2, 0, 0);
        if (result != LUA_OK)
        {
            fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        CHECK(result == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getFound");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_toboolean(L, -1) == 1);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE(
    "context:globalViewModel returns nil for a non-global or unknown name",
    "[scripting]")
{
    // The named view model must be validated as an actual global before any
    // resolution: a non-global (e.g. the main VM) name must never resolve, and
    // an unknown name must not query an out-of-range slot key.
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/global_variables_test.riv", &silver);
    REQUIRE(file != nullptr);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto mainInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(mainInstance != nullptr);
    REQUIRE(mainInstance->viewModel() != nullptr);
    // The main artboard view model is not a global.
    const std::string mainName = mainInstance->viewModel()->name();
    REQUIRE(static_cast<ViewModelType>(
                mainInstance->viewModel()->viewModelType()) !=
            ViewModelType::global);

    auto dataContext = make_rcp<DataContext>(mainInstance);

    ScriptedObjectWithFile scriptedObjectWithFile;
    scriptedObjectWithFile.setFileForScriptAsset(file.get());
    scriptedObjectWithFile.dataContext(dataContext);

    ScriptingTest vm(
        R"(
local mainResult = "not_called"
local unknownResult = "not_called"

function testNames(context: Context, mainName: string)
  mainResult = context:globalViewModel(mainName) == nil and "nil" or "found"
  unknownResult =
    context:globalViewModel("NoSuchViewModel") == nil and "nil" or "found"
end

function getMain(): string return mainResult end
function getUnknown(): string return unknownResult end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testNames");
        lua_newrive<ScriptedContext>(L, &scriptedObjectWithFile);
        lua_pushstring(L, mainName.c_str());
        CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getMain");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "nil");
        lua_pop(L, 1);
    }

    {
        lua_getglobal(L, "getUnknown");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "nil");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:dataContext returns nil with no data context", "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
local result = "not_called"

function testDataContext(context: Context)
  local dc = context:dataContext()
  if dc == nil then
    result = "nil"
  else
    result = "found"
  end
end

function getResult(): string
  return result
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testDataContext");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getResult");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "nil");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:features returns fallback table without RIVE_CANVAS",
          "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
local featuresTable = nil

function testFeatures(context: Context)
  featuresTable = context:features()
end

function getFeatures()
  return featuresTable
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testFeatures");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Verify features table was returned with expected fallback values
    {
        lua_getglobal(L, "getFeatures");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_istable(L, -1));

        // Check boolean fields default to false
        lua_getfield(L, -1, "bc");
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 1);

        lua_getfield(L, -1, "etc2");
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 1);

        lua_getfield(L, -1, "astc");
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 1);

        lua_getfield(L, -1, "anisotropicFiltering");
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 1);

        lua_getfield(L, -1, "texture3D");
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 1);

        // Check numeric fields have expected defaults
        lua_getfield(L, -1, "maxTextureSize2D");
        CHECK(lua_tonumber(L, -1) == 4096);
        lua_pop(L, 1);

        lua_getfield(L, -1, "maxTextureSizeCube");
        CHECK(lua_tonumber(L, -1) == 4096);
        lua_pop(L, 1);

        lua_getfield(L, -1, "maxTextureSize3D");
        CHECK(lua_tonumber(L, -1) == 256);
        lua_pop(L, 1);

        lua_getfield(L, -1, "maxColorAttachments");
        CHECK(lua_tonumber(L, -1) == 4);
        lua_pop(L, 1);

        lua_getfield(L, -1, "maxUniformBufferSize");
        CHECK(lua_tonumber(L, -1) == 16384);
        lua_pop(L, 1);

        lua_getfield(L, -1, "maxSamplers");
        CHECK(lua_tonumber(L, -1) == 16);
        lua_pop(L, 1);

        lua_getfield(L, -1, "maxSamples");
        CHECK(lua_tonumber(L, -1) == 4);
        lua_pop(L, 1);

        lua_pop(L, 1); // pop table
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:preferredCanvasFormat is removed", "[scripting]")
{
    // Removed in favor of canvas.format, which reports the speculative format
    // even before a deferred canvas is resized.
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
function testRemoved(context: Context)
  context:preferredCanvasFormat()
end
)",
        1,
        true); // errorOk = true

    lua_State* L = vm.state();

    lua_getglobal(L, "testRemoved");
    lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
    int result = lua_pcall(L, 1, 0, 0);
    CHECK(result != LUA_OK);
    const char* error = lua_tostring(L, -1);
    CHECK(std::string(error).find("is not a valid method") !=
          std::string::npos);
    lua_pop(L, 1);
}

TEST_CASE("context invalid method raises error", "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
function testInvalidMethod(context: Context)
  context:thisMethodDoesNotExist()
end
)",
        1,
        true); // errorOk = true

    lua_State* L = vm.state();

    lua_getglobal(L, "testInvalidMethod");
    lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
    int result = lua_pcall(L, 1, 0, 0);
    CHECK(result != LUA_OK);
    // Error message should mention invalid method
    const char* error = lua_tostring(L, -1);
    CHECK(std::string(error).find("is not a valid method") !=
          std::string::npos);
    lua_pop(L, 1);
}

TEST_CASE("context:blob returns nil when no script asset", "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
local result = "not_called"

function testBlob(context: Context)
  local b = context:blob("anyname")
  if b == nil then
    result = "nil"
  else
    result = "found"
  end
end

function getResult(): string
  return result
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testBlob");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getResult");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "nil");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:blob returns nil for non-existent blob", "[scripting]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/walle.riv", &silver);
    REQUIRE(file != nullptr);

    ScriptedObjectWithFile scriptedObjectWithFile;
    scriptedObjectWithFile.setFileForScriptAsset(file.get());

    ScriptingTest vm(
        R"(
local result = "not_called"

function testBlob(context: Context)
  local b = context:blob("nonexistent_blob")
  if b == nil then
    result = "nil"
  else
    result = "found"
  end
end

function getResult(): string
  return result
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testBlob");
        lua_newrive<ScriptedContext>(L, &scriptedObjectWithFile);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getResult");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "nil");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("context:markNeedsUpdate on context without dataContext",
          "[scripting]")
{
    // markNeedsUpdate should work regardless of dataContext
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
function testMark(context: Context)
  context:markNeedsUpdate()
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testMark");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
        CHECK(scriptedObjectTest.needsUpdate());
    }
}

TEST_CASE("ScriptingContext ore/render context default to null", "[scripting]")
{
    // Guard against regression where ore/render context were static globals.
    // A freshly created VM must start with null ore and render context — not
    // inheriting any previously set value from another VM or a prior run.
    ScriptingTest vm("-- empty");
    ScriptingContext* ctx = vm.vm()->context();
    REQUIRE(ctx != nullptr);
    CHECK(ctx->oreContext() == nullptr);
    CHECK(ctx->renderContext() == nullptr);
}

// Script calls nest: a ScriptedDrawable that draws an artboard into its canvas
// runs that artboard's own scripted drawables inside its open canvas frame. The
// post-call cleanup used to take the whole open-frame list, so the inner call's
// return closed the frame the outer call was still drawing into -- the outer
// renderer went invalid mid-draw and its canvas composited nothing.
TEST_CASE("open canvas frames are reclaimed per call, not wholesale",
          "[scripting]")
{
    rive::CPPRuntimeScriptingContext context(&gNoOpFactory);

    // The frame an enclosing draw opened, and the token a nested call takes.
    context.registerOpenCanvasFrame(11);
    uint64_t token = context.nextOpenCanvasFrameToken();

    // The nested call opens one of its own and returns without ending it.
    context.registerOpenCanvasFrame(22);
    auto leaked = context.takeOpenCanvasFramesFrom(token);
    REQUIRE(leaked.size() == 1);
    CHECK(leaked[0] == 22);
    CHECK(context.openCanvasFrameCount() == 1);

    // The enclosing frame is still open, and is reclaimed by its own call.
    auto outer = context.takeOpenCanvasFramesFrom(0);
    REQUIRE(outer.size() == 1);
    CHECK(outer[0] == 11);
}

// A nested call may end a frame it inherited and open one of its own, leaving
// the list exactly as long as it found it. A positional mark reads that as
// "nothing past the end" and hands the nested call's frame back to the
// enclosing one, which then never closes it.
TEST_CASE(
    "a nested call that swaps out an inherited frame still reclaims its own",
    "[scripting]")
{
    rive::CPPRuntimeScriptingContext context(&gNoOpFactory);
    context.registerOpenCanvasFrame(11);
    uint64_t token = context.nextOpenCanvasFrameToken();

    context.unregisterOpenCanvasFrame(11); // the nested call ends it
    context.registerOpenCanvasFrame(22);   // ...and opens its own
    REQUIRE(context.openCanvasFrameCount() == 1);

    auto leaked = context.takeOpenCanvasFramesFrom(token);
    REQUIRE(leaked.size() == 1);
    CHECK(leaked[0] == 22);
    CHECK(context.openCanvasFrameCount() == 0);
}

// The same shape one level deeper: frames the enclosing call opened both before
// and after an inherited one was ended stay put, and only the nested call's do
// not.
TEST_CASE("reclaiming a nested call's frames leaves interleaved outer ones",
          "[scripting]")
{
    rive::CPPRuntimeScriptingContext context(&gNoOpFactory);
    context.registerOpenCanvasFrame(11);
    context.registerOpenCanvasFrame(12);
    uint64_t token = context.nextOpenCanvasFrameToken();

    context.unregisterOpenCanvasFrame(11);
    context.registerOpenCanvasFrame(22);
    context.registerOpenCanvasFrame(23);

    auto leaked = context.takeOpenCanvasFramesFrom(token);
    REQUIRE(leaked.size() == 2);
    CHECK(leaked[0] == 22);
    CHECK(leaked[1] == 23);
    REQUIRE(context.openCanvasFrameCount() == 1);
    auto outer = context.takeOpenCanvasFramesFrom(0);
    REQUIRE(outer.size() == 1);
    CHECK(outer[0] == 12);
}

// Nothing open, and a token from before anything was ever registered, both
// reclaim nothing rather than walking the list.
TEST_CASE("reclaiming from an empty open frame list is a no-op", "[scripting]")
{
    rive::CPPRuntimeScriptingContext context(&gNoOpFactory);
    uint64_t token = context.nextOpenCanvasFrameToken();
    CHECK(context.takeOpenCanvasFramesFrom(token).empty());
    CHECK(context.takeOpenCanvasFramesFrom(0).empty());
    CHECK(context.openCanvasFrameCount() == 0);
}

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
namespace
{
// A pass that records nothing but tracks whether it was finished, which is the
// only thing the post-call cleanup does to one.
class StubRenderPass : public rive::ore::RenderPass
{
public:
    void setPipeline(rive::ore::Pipeline*) override {}
    void setVertexBuffer(uint32_t, rive::ore::Buffer*, uint32_t) override {}
    void setIndexBuffer(rive::ore::Buffer*,
                        rive::ore::IndexFormat,
                        uint32_t) override
    {}
    void setBindGroup(uint32_t,
                      rive::ore::BindGroup*,
                      const uint32_t*,
                      uint32_t) override
    {}
    void setViewport(float, float, float, float, float, float) override {}
    void setScissorRect(uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void setStencilReference(uint32_t) override {}
    void setBlendColor(float, float, float, float) override {}
    void draw(uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void drawIndexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override
    {}
    void finish() override { m_finished = true; }
};

// Enough of an ore::Context to carry the active-pass slot the scope reads.
class StubOreContext : public rive::ore::Context
{
public:
    StubOreContext() : rive::ore::Context(nullptr) {}

    rive::rcp<rive::ore::Buffer> makeBuffer(
        const rive::ore::BufferDesc&) override
    {
        return nullptr;
    }
    rive::rcp<rive::ore::Texture> makeTexture(
        const rive::ore::TextureDesc&) override
    {
        return nullptr;
    }
    rive::rcp<rive::ore::TextureView> makeTextureView(
        const rive::ore::TextureViewDesc&) override
    {
        return nullptr;
    }
    rive::rcp<rive::ore::Sampler> makeSampler(
        const rive::ore::SamplerDesc&) override
    {
        return nullptr;
    }
    rive::rcp<rive::ore::ShaderModule> makeShaderModule(
        const rive::ore::ShaderModuleDesc&) override
    {
        return nullptr;
    }
    rive::rcp<rive::ore::BindGroupLayout> makeBindGroupLayout(
        const rive::ore::BindGroupLayoutDesc&) override
    {
        return nullptr;
    }
    rive::rcp<rive::ore::Pipeline> makePipeline(const rive::ore::PipelineDesc&,
                                                std::string*) override
    {
        return nullptr;
    }
    rive::rcp<rive::ore::BindGroup> makeBindGroup(
        const rive::ore::BindGroupDesc&) override
    {
        return nullptr;
    }
    std::unique_ptr<rive::ore::RenderPass> beginRenderPass(
        const rive::ore::RenderPassDesc&,
        std::string*) override
    {
        return nullptr;
    }
    void beginFrame(const FrameDescriptor&) override {}
    void endFrame() override {}
    void waitForGPU() override {}
    rive::rcp<rive::ore::TextureView> wrapCanvasTexture(
        rive::gpu::RenderCanvas*) override
    {
        return nullptr;
    }
    rive::rcp<rive::ore::TextureView> wrapRiveTexture(rive::gpu::Texture*,
                                                      uint32_t,
                                                      uint32_t) override
    {
        return nullptr;
    }
    rive::ore::ShaderTarget shaderTarget() const override
    {
        return rive::ore::ShaderTarget::wgsl;
    }
};

// Routes the VM's oreContext() to the stub, the way a recording session's
// factory routes it to the context it records for.
class StubOreFactory : public rive::NoOpFactory
{
public:
    StubOreContext oreContext;
    rive::ore::Context* ore() override { return &oreContext; }
};

// What the C function below claims when rive_lua_pcall runs it. Standing in
// for the inner script call, it is the only party that may leave GPU work
// open once it returns. Single threaded, one test at a time.
rive::ScriptingContext* gNestedContext = nullptr;
StubOreContext* gNestedOre = nullptr;
StubRenderPass* gNestedPass = nullptr;
bool gNestedOpensCanvasFrame = false;

// Stands in for the inner ScriptedDrawable's callback: rive_lua_pcall wraps
// it exactly as it wraps a script's draw, so calling it drives the real
// enter/exit wiring rather than the scope helpers in isolation.
int nestedScriptCall(lua_State* L)
{
    if (gNestedPass != nullptr)
    {
        gNestedOre->setActiveRenderPass(gNestedPass);
    }
    if (gNestedOpensCanvasFrame)
    {
        // Idle, so the cleanup releases the ref without ending a frame.
        rive::lua_newrive<rive::ScriptedCanvas>(L);
        int ref = lua_ref(L, -1);
        lua_pop(L, 1);
        gNestedContext->registerOpenCanvasFrame(ref);
    }
    return 0;
}
} // namespace

// The pass half of the same nesting problem: an inner script call returning
// must not finish the render pass the enclosing call is still recording into.
// Finishing it here is what left the outer draw issuing against a dead
// encoder.
TEST_CASE("a nested call leaves the render pass it inherited open",
          "[scripting]")
{
    StubOreFactory factory;
    ScriptingTest vm("-- empty", 1, false, {}, true, &factory);
    lua_State* L = vm.state();
    REQUIRE(vm.vm()->context()->oreContext() ==
            static_cast<void*>(&factory.oreContext));

    StubRenderPass outerPass;
    factory.oreContext.setActiveRenderPass(&outerPass);

    // A nested script call begins and returns without touching the pass.
    ScriptCallGpuScope scope = rive_lua_enterScriptCallGpuScope(L);
    rive_lua_exitScriptCallGpuScope(L, scope);

    CHECK_FALSE(outerPass.isFinished());
    CHECK(factory.oreContext.activeRenderPass() == &outerPass);
    CHECK(vm.errors.empty());
}

// The leak the cleanup is there for is still caught: a pass the call itself
// opened and forgot is finished, cleared, and reported.
TEST_CASE("a call's own abandoned render pass is finished and cleared",
          "[scripting]")
{
    StubOreFactory factory;
    ScriptingTest vm("-- empty", 1, false, {}, true, &factory);
    lua_State* L = vm.state();

    ScriptCallGpuScope scope = rive_lua_enterScriptCallGpuScope(L);
    StubRenderPass ownPass;
    factory.oreContext.setActiveRenderPass(&ownPass);
    rive_lua_exitScriptCallGpuScope(L, scope);

    CHECK(ownPass.isFinished());
    CHECK(factory.oreContext.activeRenderPass() == nullptr);
    REQUIRE(vm.errors.size() == 1);
    CHECK(vm.errors[0] ==
          std::string("GPU render pass left open at script return. "
                      "Call :finish() on render passes before returning."));
}

// Both at once, which is the shape the repro file hits: the nested call opens
// its own pass over the enclosing one. Its pass is reclaimed; the enclosing
// call's pass object is left for that call to finish itself.
TEST_CASE("a nested call's pass is reclaimed without finishing the outer one",
          "[scripting]")
{
    StubOreFactory factory;
    ScriptingTest vm("-- empty", 1, false, {}, true, &factory);
    lua_State* L = vm.state();

    StubRenderPass outerPass;
    factory.oreContext.setActiveRenderPass(&outerPass);

    ScriptCallGpuScope scope = rive_lua_enterScriptCallGpuScope(L);
    StubRenderPass innerPass;
    factory.oreContext.setActiveRenderPass(&innerPass);
    rive_lua_exitScriptCallGpuScope(L, scope);

    CHECK(innerPass.isFinished());
    CHECK_FALSE(outerPass.isFinished());
    CHECK(factory.oreContext.activeRenderPass() == nullptr);
}

// An already finished pass is nobody's leak, so the cleanup reports nothing
// and leaves the slot for whoever owns it.
TEST_CASE("a finished render pass is not reported as left open", "[scripting]")
{
    StubOreFactory factory;
    ScriptingTest vm("-- empty", 1, false, {}, true, &factory);
    lua_State* L = vm.state();

    ScriptCallGpuScope scope = rive_lua_enterScriptCallGpuScope(L);
    StubRenderPass ownPass;
    factory.oreContext.setActiveRenderPass(&ownPass);
    ownPass.finish();
    rive_lua_exitScriptCallGpuScope(L, scope);

    CHECK(factory.oreContext.activeRenderPass() == &ownPass);
    CHECK(vm.errors.empty());
}

// The scope helpers above are only correct if rive_lua_pcall actually brackets
// the call with them. These drive the wiring itself: nestedScriptCall goes
// through rive_lua_pcall the same way a ScriptedDrawable's draw does.
TEST_CASE("rive_lua_pcall leaves the caller's GPU work alone", "[scripting]")
{
    StubOreFactory factory;
    ScriptingTest vm("-- empty", 1, false, {}, true, &factory);
    lua_State* L = vm.state();
    ScriptingContext* context = vm.vm()->context();

    // The enclosing draw's pass and canvas frame.
    StubRenderPass outerPass;
    factory.oreContext.setActiveRenderPass(&outerPass);
    lua_newrive<ScriptedCanvas>(L);
    int outerFrame = lua_ref(L, -1);
    lua_pop(L, 1);
    context->registerOpenCanvasFrame(outerFrame);

    // A nested call that opens nothing of its own.
    gNestedContext = context;
    gNestedOre = &factory.oreContext;
    gNestedPass = nullptr;
    gNestedOpensCanvasFrame = false;
    lua_pushcfunction(L, nestedScriptCall, "nestedScriptCall");
    CHECK(rive_lua_pcall(L, 0, 0) == LUA_OK);
    gNestedContext = nullptr;
    gNestedOre = nullptr;

    CHECK_FALSE(outerPass.isFinished());
    CHECK(factory.oreContext.activeRenderPass() == &outerPass);
    CHECK(context->openCanvasFrameCount() == 1);
    CHECK(vm.errors.empty());
}

TEST_CASE("rive_lua_pcall reclaims only what the nested call opened",
          "[scripting]")
{
    StubOreFactory factory;
    ScriptingTest vm("-- empty", 1, false, {}, true, &factory);
    lua_State* L = vm.state();
    ScriptingContext* context = vm.vm()->context();

    StubRenderPass outerPass;
    factory.oreContext.setActiveRenderPass(&outerPass);
    lua_newrive<ScriptedCanvas>(L);
    int outerFrame = lua_ref(L, -1);
    lua_pop(L, 1);
    context->registerOpenCanvasFrame(outerFrame);

    // The nested call opens its own pass and canvas frame and forgets both.
    StubRenderPass innerPass;
    gNestedContext = context;
    gNestedOre = &factory.oreContext;
    gNestedPass = &innerPass;
    gNestedOpensCanvasFrame = true;
    lua_pushcfunction(L, nestedScriptCall, "nestedScriptCall");
    CHECK(rive_lua_pcall(L, 0, 0) == LUA_OK);
    gNestedContext = nullptr;
    gNestedOre = nullptr;
    gNestedPass = nullptr;
    gNestedOpensCanvasFrame = false;

    // Reclaimed: the nested call's own work, reported as left open.
    CHECK(innerPass.isFinished());
    CHECK(factory.oreContext.activeRenderPass() == nullptr);
    CHECK(vm.errors.size() == 2);

    // Untouched: the enclosing draw's, which it is still using.
    CHECK_FALSE(outerPass.isFinished());
    CHECK(context->openCanvasFrameCount() == 1);
}
#endif // RIVE_CANVAS && RIVE_ORE
