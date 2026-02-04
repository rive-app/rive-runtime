
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive_file_reader.hpp"

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