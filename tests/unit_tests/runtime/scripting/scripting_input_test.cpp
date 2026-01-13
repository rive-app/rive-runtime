
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_layout.hpp"
#include "rive/scripted/scripted_data_converter.hpp"
#include "utils/no_op_renderer.hpp"
#include "rive/layout/layout_enums.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive_file_reader.hpp"
#include <memory>

using namespace rive;

TEST_CASE("scripted object can set and read number input", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyNode = {
  myNumber: Input<number>,
}

function init(self: MyNode, context: Context): boolean
  return true
end

function advance(self: MyNode, seconds: number): boolean
  return true
end

return function(): Node<MyNode>
  return {
    init = init,
    advance = advance,
    myNumber = 42.0,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    // ScriptingTest executes the code and leaves the factory function on the
    // stack
    REQUIRE(top >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Call the factory function to get the actual table
    CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
    top = lua_gettop(L);
    REQUIRE(top == 1);

    // Test reading initial value directly from table
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "myNumber");
        CHECK(lua_type(L, -1) == LUA_TNUMBER);
        CHECK(lua_tonumber(L, -1) == 42.0);
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Test setting value via ScriptedObject pattern
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_pushnumber(L, 100.5);
        lua_setfield(L, -2, "myNumber");
        lua_pop(L, 1); // Pop duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Verify the value was set
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "myNumber");
        CHECK(lua_type(L, -1) == LUA_TNUMBER);
        CHECK(lua_tonumber(L, -1) == 100.5);
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted object can set and read string input", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyNode = {
  myString: Input<string>,
}

function init(self: MyNode, context: Context): boolean
  return true
end

function advance(self: MyNode, seconds: number): boolean
  return true
end

function draw(self: MyNode, renderer: Renderer)
end

return function(): Node<MyNode>
  return {
    init = init,
    advance = advance,
    draw = draw,
    myString = 'initial',
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    // ScriptingTest executes the code and leaves the factory function on the
    // stack
    REQUIRE(top >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Call the factory function to get the actual table
    CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
    top = lua_gettop(L);
    REQUIRE(top == 1);

    // Test reading initial value directly from table
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "myString");
        CHECK(lua_type(L, -1) == LUA_TSTRING);
        CHECK(std::string(lua_tostring(L, -1)) == "initial");
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Test setting value
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_pushstring(L, "updated");
        lua_setfield(L, -2, "myString");
        lua_pop(L, 1); // Pop duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Verify the value was set
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "myString");
        CHECK(lua_type(L, -1) == LUA_TSTRING);
        CHECK(std::string(lua_tostring(L, -1)) == "updated");
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted object can set and read boolean input", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyNode = {
  myBoolean: Input<boolean>,
}

function init(self: MyNode, context: Context): boolean
  return true
end

function advance(self: MyNode, seconds: number): boolean
  return true
end

return function(): Node<MyNode>
  return {
    init = init,
    advance = advance,
    myBoolean = false,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    // ScriptingTest executes the code and leaves the factory function on the
    // stack
    REQUIRE(top >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Call the factory function to get the actual table
    CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
    top = lua_gettop(L);
    REQUIRE(top == 1);

    // Test reading initial value directly from table
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "myBoolean");
        CHECK(lua_type(L, -1) == LUA_TBOOLEAN);
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Test setting value
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "myBoolean");
        lua_pop(L, 1); // Pop duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Verify the value was set
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "myBoolean");
        CHECK(lua_type(L, -1) == LUA_TBOOLEAN);
        CHECK(lua_toboolean(L, -1) == 1);
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted object can set and read integer input", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyNode = {
  myInteger: Input<number>,
}

function init(self: MyNode, context: Context): boolean
  return true
end

function advance(self: MyNode, seconds: number): boolean
  return true
end

return function(): Node<MyNode>
  return {
    init = init,
    advance = advance,
    myInteger = 10,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    // ScriptingTest executes the code and leaves the factory function on the
    // stack
    REQUIRE(top >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Call the factory function to get the actual table
    CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
    top = lua_gettop(L);
    REQUIRE(top == 1);

    // Test reading initial value directly from table
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "myInteger");
        CHECK(lua_type(L, -1) == LUA_TNUMBER);
        CHECK(lua_tonumber(L, -1) == 10.0);
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Test setting value using unsigned (for integers)
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_pushunsigned(L, 99);
        lua_setfield(L, -2, "myInteger");
        lua_pop(L, 1); // Pop duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Verify the value was set
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "myInteger");
        CHECK(lua_type(L, -1) == LUA_TNUMBER);
        CHECK(lua_tonumber(L, -1) == 99.0);
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted object can use multiple inputs", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyNode = {
  count: Input<number>,
  label: Input<string>,
  enabled: Input<boolean>,
}

function init(self: MyNode, context: Context): boolean
  return true
end

function advance(self: MyNode, seconds: number): boolean
  return true
end

function compute(self: MyNode): string
  if self.enabled then
    return self.label .. ": " .. tostring(self.count)
  else
    return "disabled"
  end
end

return function(): Node<MyNode>
  return {
    init = init,
    advance = advance,
    count = 0,
    label = "default",
    enabled = true,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    // ScriptingTest executes the code and leaves the factory function on the
    // stack
    REQUIRE(top >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Call the factory function to get the actual table
    CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
    top = lua_gettop(L);
    REQUIRE(top == 1);

    // Test initial values directly from table
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "count");
        CHECK(lua_tonumber(L, -1) == 0.0);
        lua_pop(L, 1); // Pop count

        lua_getfield(L, -1, "label");
        CHECK(std::string(lua_tostring(L, -1)) == "default");
        lua_pop(L, 1); // Pop label

        lua_getfield(L, -1, "enabled");
        CHECK(lua_toboolean(L, -1) == 1);
        lua_pop(L, 2); // Pop enabled and duplicated self

        CHECK(top == lua_gettop(L));
    }

    // Set multiple inputs
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_pushnumber(L, 42.0);
        lua_setfield(L, -2, "count");
        lua_pushstring(L, "test");
        lua_setfield(L, -2, "label");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "enabled");
        lua_pop(L, 1); // Pop duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Verify values were set
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "count");
        CHECK(lua_tonumber(L, -1) == 42.0);
        lua_pop(L, 1); // Pop count

        lua_getfield(L, -1, "label");
        CHECK(std::string(lua_tostring(L, -1)) == "test");
        lua_pop(L, 1); // Pop label

        lua_getfield(L, -1, "enabled");
        CHECK(lua_toboolean(L, -1) == 0);
        lua_pop(L, 1); // Pop enabled

        // Test computation using inputs
        lua_getglobal(L, "compute");
        lua_pushvalue(L, -2); // Push self (original table)
        CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "disabled");
        lua_pop(L, 2); // Pop result and duplicated self

        CHECK(top == lua_gettop(L));
    }

    // Re-enable and verify computation
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "enabled");
        lua_pop(L, 1); // Pop duplicated self

        lua_getglobal(L, "compute");
        lua_pushvalue(L, -2); // Push self
        CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
        CHECK(std::string(lua_tostring(L, -1)) == "test: 42");
        lua_pop(L, 1); // Pop result

        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted object inputs can be used in advance function",
          "[scripting]")
{
    ScriptingTest vm(
        R"(type MyNode = {
  speed: Input<number>,
  position: Input<number>,
}

function init(self: MyNode, context: Context): boolean
  self.position = 0
  return true
end

function advance(self: MyNode, seconds: number): boolean
  self.position = self.position + self.speed * seconds
  return true
end

return function(): Node<MyNode>
  return {
    init = init,
    advance = advance,
    speed = 10.0,
    position = 0,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    // ScriptingTest executes the code and leaves the factory function on the
    // stack
    REQUIRE(top >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Call the factory function to get the actual table
    CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
    top = lua_gettop(L);
    REQUIRE(top == 1);

    // Set speed input
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_pushnumber(L, 50.0);
        lua_setfield(L, -2, "speed");
        lua_pop(L, 1); // Pop duplicated self
        CHECK(top == lua_gettop(L));
    }

    // Advance multiple times
    {
        lua_getglobal(L, "advance");
        lua_pushvalue(L, -2);   // Push self
        lua_pushnumber(L, 0.1); // 0.1 seconds
        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        CHECK(lua_toboolean(L, -1) == 1);
        lua_pop(L, 1); // Pop result
        CHECK(top == lua_gettop(L));
    }

    {
        lua_getglobal(L, "advance");
        lua_pushvalue(L, -2);   // Push self
        lua_pushnumber(L, 0.1); // Another 0.1 seconds
        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        lua_pop(L, 1); // Pop result
        CHECK(top == lua_gettop(L));
    }

    // Verify position was updated using speed input
    {
        lua_pushvalue(L, -1); // Duplicate self table
        lua_getfield(L, -1, "position");
        CHECK(lua_tonumber(L, -1) == Approx(10.0)); // 50.0 * 0.1 * 2 = 10.0
        lua_pop(L, 2); // Pop value and duplicated self
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("ScriptedDrawable can be created and initialized", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyNode = {
  count: Input<number>,
}

function init(self: MyNode, context: Context): boolean
  return true
end

function advance(self: MyNode, seconds: number): boolean
  return true
end

function draw(self: MyNode, renderer: Renderer)
end

return function(): Node<MyNode>
  return {
    init = init,
    advance = advance,
    draw = draw,
    count = 0,
  }
end
)");
    lua_State* L = vm.state();
    REQUIRE(lua_gettop(L) >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Create ScriptedDrawable instance
    ScriptedDrawable drawable;

    // Create LuaState from ScriptingTest's VM state
    lua_State* luaState = L;

    // Manually set the inits flag since we're not using ScriptAsset
    // The inits flag indicates that the script has an init function
    drawable.implementedMethods(drawable.implementedMethods() |
                                (1 << 9)); // m_initsBit

    // scriptInit expects the factory function on the stack
    // ScriptingTest already left it there, so we can call scriptInit directly
    // Call ScriptedObject::scriptInit directly to avoid addDirt which requires
    // an artboard
    CHECK(drawable.ScriptedObject::scriptInit(luaState));

    // Manually set implemented methods flags since we're not using ScriptAsset
    // Check if advance function exists and set the flag
    // After scriptInit, the self table is stored in m_self, so we can push it
    {
        rive_lua_pushRef(L, drawable.self());
        lua_getfield(L, -1, "advance");
        if (lua_type(L, -1) == LUA_TFUNCTION)
        {
            drawable.implementedMethods(drawable.implementedMethods() |
                                        (1 << 0)); // m_advancesBit
        }
        lua_pop(L, 2); // Pop function check and self table
    }

    // Test that we can set and get inputs
    drawable.setNumberInput("count", 42.0);

    // Test advance - need AdvanceNested flag for it to work
    CHECK(drawable.advanceComponent(0.1f,
                                    AdvanceFlags::AdvanceNested |
                                        AdvanceFlags::Animate |
                                        AdvanceFlags::NewFrame));

    // Test draw
    NoOpRenderer renderer;
    drawable.draw(&renderer);

    // Cleanup
    drawable.scriptDispose();
}

TEST_CASE("ScriptedLayout can be created and initialized", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyLayout = {
  width: Input<number>,
}

function init(self: MyLayout, context: Context): boolean
  return true
end

function advance(self: MyLayout, seconds: number): boolean
  return true
end

function measure(self: MyLayout): Vector
  return Vector.xy(200, 150)
end

function resize(self: MyLayout, size: Vector)
end

return function(): Layout<MyLayout>
  return {
    init = init,
    advance = advance,
    measure = measure,
    resize = resize,
    width = 100,
  }
end
)");
    lua_State* L = vm.state();
    REQUIRE(lua_gettop(L) >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Create ScriptedLayout instance
    ScriptedLayout layout;

    // Create LuaState from ScriptingTest's VM state
    lua_State* luaState = L;

    // Manually set the inits flag since we're not using ScriptAsset
    layout.implementedMethods(layout.implementedMethods() |
                              (1 << 9)); // m_initsBit

    // scriptInit expects the factory function on the stack
    // Call ScriptedObject::scriptInit directly to avoid addDirt which requires
    // an artboard
    CHECK(layout.ScriptedObject::scriptInit(luaState));

    // Manually set implemented methods flags
    // After scriptInit, the self table is stored in m_self
    {
        rive_lua_pushRef(L, layout.self());
        lua_getfield(L, -1, "advance");
        if (lua_type(L, -1) == LUA_TFUNCTION)
        {
            layout.implementedMethods(layout.implementedMethods() |
                                      (1 << 0)); // m_advancesBit
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "measure");
        if (lua_type(L, -1) == LUA_TFUNCTION)
        {
            layout.implementedMethods(layout.implementedMethods() |
                                      (1 << 2)); // m_measuresBit
        }
        lua_pop(L, 2); // Pop function check and self table
    }

    // Test that we can set inputs
    layout.setNumberInput("width", 300.0);

    // Test measureLayout
    Vec2D measured = layout.measureLayout(0,
                                          LayoutMeasureMode::undefined,
                                          0,
                                          LayoutMeasureMode::undefined);
    CHECK(measured.x == 200.0f);
    CHECK(measured.y == 150.0f);

    // Test controlSize
    layout.controlSize(Vec2D(250, 200),
                       LayoutScaleType::fill,
                       LayoutScaleType::fill,
                       LayoutDirection::ltr);

    // Test advance - need AdvanceNested flag
    CHECK(layout.advanceComponent(0.1f,
                                  AdvanceFlags::AdvanceNested |
                                      AdvanceFlags::Animate |
                                      AdvanceFlags::NewFrame));

    // Cleanup
    layout.scriptDispose();
}

TEST_CASE("ScriptedDataConverter can be created and initialized", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyConverter = {
  multiplier: Input<number>,
}

function init(self: MyConverter, context: Context): boolean
  return true
end

function advance(self: MyConverter, seconds: number): boolean
  return true
end

function convert(self: MyConverter, input: DataValueNumber): DataValueNumber
  local result = DataValue.number()
  result.value = input.value * self.multiplier
  return result
end

function reverseConvert(self: MyConverter, input: DataValueNumber): DataValueNumber
  local result = DataValue.number()
  result.value = input.value / self.multiplier
  return result
end

return function(): Converter<MyConverter, DataValueNumber, DataValueNumber>
  return {
    init = init,
    advance = advance,
    convert = convert,
    reverseConvert = reverseConvert,
    multiplier = 2.0,
  }
end
)");
    lua_State* L = vm.state();
    REQUIRE(L != nullptr);

    // Ensure stack is clean and has the factory function
    int stackTop = lua_gettop(L);
    REQUIRE(stackTop >= 1);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    // Create ScriptedDataConverter instance on heap to avoid potential stack
    // alignment issues Note: ScriptedDataConverter has multiple inheritance
    // which might cause memory layout issues when tests run together
    auto converter = std::make_unique<ScriptedDataConverter>();

    // Create LuaState from ScriptingTest's VM state
    // Keep it in scope for the entire test since converter stores a pointer to
    // it
    lua_State* luaState = L;

    // Manually set the inits flag since we're not using ScriptAsset
    converter->implementedMethods(converter->implementedMethods() |
                                  (1 << 9)); // m_initsBit

    // Verify LuaState is valid before using it
    REQUIRE(luaState != nullptr);
    REQUIRE(luaState == L);

    // scriptInit expects the factory function on the stack
    // Verify stack state before calling scriptInit
    REQUIRE(lua_gettop(L) == stackTop);
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    REQUIRE(converter->ScriptedObject::scriptInit(luaState));
    converter->addScriptedDirt(ComponentDirt::Bindings);

    // Verify self() is valid before using it
    REQUIRE(converter->self() != 0);

    // Manually set implemented methods flags
    // After scriptInit, the self table is stored in m_self
    {
        rive_lua_pushRef(L, converter->self());
        REQUIRE(lua_type(L, -1) == LUA_TTABLE); // Verify we got a valid table
        lua_getfield(L, -1, "advance");
        if (lua_type(L, -1) == LUA_TFUNCTION)
        {
            converter->implementedMethods(converter->implementedMethods() |
                                          (1 << 0)); // m_advancesBit
        }
        lua_pop(L, 2); // Pop function check and self table
    }

    // Test that we can set inputs
    converter->setNumberInput("multiplier", 3.0);

    // Test advance - need AdvanceNested flag
    // Note: luaState must still be in scope here since converter.m_state points
    // to it
    CHECK(converter->advanceComponent(0.1f,
                                      AdvanceFlags::AdvanceNested |
                                          AdvanceFlags::Animate |
                                          AdvanceFlags::NewFrame));

    // Note: Testing convert() would require creating DataValueNumber and
    // DataBind objects, which is more complex. The initialization test verifies
    // the basic setup works.

    // Cleanup - must happen before luaState goes out of scope
    converter->scriptDispose();
}

TEST_CASE("scripted input color and trigger test", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_inputs_test_1.riv", &silver);
    auto artboard = file->artboardNamed("Artboard");

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

    rive::ViewModelInstanceTrigger* trigger =
        vmi->propertyValue("Trigger")->as<rive::ViewModelInstanceTrigger>();
    REQUIRE(trigger != nullptr);

    int frames = 30;
    for (int i = 0; i < frames; i++)
    {
        trigger->trigger();
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("script_input_color_trigger"));
}