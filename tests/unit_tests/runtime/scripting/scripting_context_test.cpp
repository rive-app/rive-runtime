
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive_file_reader.hpp"
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

TEST_CASE("context:preferredCanvasFormat returns platform format",
          "[scripting]")
{
    ScriptedObjectTest scriptedObjectTest;

    ScriptingTest vm(
        R"(
local format = nil

function testFormat(context: Context)
  format = context:preferredCanvasFormat()
end

function getFormat(): string
  return format
end
)");

    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    {
        lua_getglobal(L, "testFormat");
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "getFormat");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        std::string format = lua_tostring(L, -1);
#if defined(_WIN32)
        CHECK(format == "bgra8unorm");
#else
        CHECK(format == "rgba8unorm");
#endif
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
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

TEST_CASE("ScriptingContext ore/render context are per-instance", "[scripting]")
{
    // Two independent VMs must not share ore or render context state.
    // This guards against any static/global storage of these pointers.
    ScriptingTest vmA("-- a");
    ScriptingTest vmB("-- b");

    ScriptingContext* ctxA = vmA.vm()->context();
    ScriptingContext* ctxB = vmB.vm()->context();
    REQUIRE(ctxA != nullptr);
    REQUIRE(ctxB != nullptr);
    REQUIRE(ctxA != ctxB);

    // Use distinct sentinel values so a cross-write would be detectable.
    int sentinelA = 0xA;
    int sentinelB = 0xB;
    ctxA->setOreContext(&sentinelA);
    ctxA->setRenderContext(&sentinelA);
    ctxB->setOreContext(&sentinelB);
    ctxB->setRenderContext(&sentinelB);

    // Each context must hold only its own value.
    CHECK(ctxA->oreContext() == &sentinelA);
    CHECK(ctxA->renderContext() == &sentinelA);
    CHECK(ctxB->oreContext() == &sentinelB);
    CHECK(ctxB->renderContext() == &sentinelB);
}

TEST_CASE("ScriptingContext ore context survives set/clear cycle",
          "[scripting]")
{
    ScriptingTest vm("-- empty");
    ScriptingContext* ctx = vm.vm()->context();
    REQUIRE(ctx != nullptr);

    int sentinel = 42;
    ctx->setOreContext(&sentinel);
    CHECK(ctx->oreContext() == &sentinel);

    ctx->setOreContext(nullptr);
    CHECK(ctx->oreContext() == nullptr);
}