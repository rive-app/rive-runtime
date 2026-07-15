
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_data_converter.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

TEST_CASE("scripted string converter support data types", "[scripting]")
{
    ScriptingTest vm(
        R"(type InputTypes = DataValueString | DataValueBoolean | DataValueNumber

type StringConverter = {}
function convert(self: StringConverter, input: InputTypes): DataValueString
  local inputString: string = ''
  local dv: DataValueString = DataValue.string()
  if input:isString() then
    inputString = (input :: DataValueString).value
    dv.value = (input :: DataValueString).value .. ' - suffix'
  elseif input:isBoolean() then
    if (input :: DataValueBoolean).value then
      inputString = 'True'
    else
      inputString = 'False'
    end
  elseif input:isNumber() then
    inputString = tostring((input :: DataValueNumber).value)
  end

  dv.value = inputString .. ' - suffix'
  return dv
end

function reverseConvert(
  self: StringConverter,
  input: InputTypes
): DataValueString
  local dv: DataValueString = DataValue.string()
  if input:isString() then
    dv.value = (input :: DataValueString).value
  end
  return dv
end

function init(self: StringConverter): boolean
  return true
end

return function(): Converter<StringConverter, InputTypes, DataValueString>
  return { convert = convert, reverseConvert = reverseConvert, init = init }
end

)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueString>(L, L, "input");
        CHECK(dv->isString());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isString());
        CHECK(scriptedDataValue->dataValue()->as<DataValueString>()->value() ==
              "input - suffix");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueBoolean>(L, L, true);
        CHECK(dv->isBoolean());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isString());
        CHECK(scriptedDataValue->dataValue()->as<DataValueString>()->value() ==
              "True - suffix");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueNumber>(L, L, 1);
        CHECK(dv->isNumber());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isString());
        CHECK(scriptedDataValue->dataValue()->as<DataValueString>()->value() ==
              "1 - suffix");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
    {
        lua_getglobal(L, "reverseConvert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueString>(L, L, "input as output");
        CHECK(dv->isString());
        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isString());
        CHECK(scriptedDataValue->dataValue()->as<DataValueString>()->value() ==
              "input as output");
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted number converter support data types", "[scripting]")
{
    ScriptingTest vm(
        R"(type NumberConverter = {}
function convert(self: NumberConverter, input: DataValueNumber): DataValueNumber
  local dv: DataValueNumber = DataValue.number()
  if input:isNumber() then
    dv.value = (input :: DataValueNumber).value + 250
  end
  return dv
end

function reverseConvert(
  self: NumberConverter,
  input: DataValueNumber
): DataValueNumber
  local dv: DataValueNumber = DataValue.number()
  return dv
end

function init(self: NumberConverter): boolean
  return true
end

return function(): Converter<NumberConverter, DataValueNumber, DataValueNumber>
  return { convert = convert, reverseConvert = reverseConvert, init = init }
end

)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueNumber>(L, L, 1);
        CHECK(dv->isNumber());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isNumber());
        CHECK(scriptedDataValue->dataValue()->as<DataValueNumber>()->value() ==
              251);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted boolean converts bool", "[scripting]")
{
    ScriptingTest vm(
        R"(type BoolConverter = {
  coin: Input<Artboard<Data.Coin>>,
}

function convert(self: BoolConverter, input: DataValueBoolean): DataValueBoolean
  local dv: DataValueBoolean = DataValue.boolean()
  dv.value = not input.value
  return dv
end

function reverseConvert(
  self: BoolConverter,
  input: DataValueBoolean
): DataValueBoolean
  local dv: DataValueBoolean = DataValue.boolean()
  return dv
end

function init(self: BoolConverter): boolean
  return true
end

return function(): Converter<BoolConverter, DataValueBoolean, DataValueBoolean>
  return {
    convert = convert,
    coin = late(),
    reverseConvert = reverseConvert,
    init = init,
  }
end

)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueBoolean>(L, L, true);
        CHECK(dv->isBoolean());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isBoolean());
        CHECK(scriptedDataValue->dataValue()->as<DataValueBoolean>()->value() ==
              false);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueBoolean>(L, L, false);
        CHECK(dv->isBoolean());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isBoolean());
        CHECK(scriptedDataValue->dataValue()->as<DataValueBoolean>()->value() ==
              true);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted color converts color value", "[scripting]")
{
    ScriptingTest vm(
        R"(type ColorConverter = {}
function convert(self: ColorConverter, input: DataValueColor): DataValueColor
  local dv: DataValueColor = DataValue.color()
  dv.value = input.value
  dv.red = 0
  dv.blue = 255
  return dv
end
function reverseConvert(
  self: ColorConverter,
  input: DataValueColor
): DataValueColor
  local dv: DataValueColor = DataValue.color()
  dv.value = input.value
  return dv
end
function init(self: ColorConverter): boolean
  return true
end
return function(): Converter<ColorConverter, DataValueColor, DataValueColor>
  return { convert = convert, reverseConvert = reverseConvert, init = init }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueColor>(L, L, 0xFFFFFF00);
        CHECK(dv->isColor());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isColor());
        CHECK(scriptedDataValue->dataValue()->as<DataValueColor>()->value() ==
              0xFF00FFFF);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueColor>(L, L, 0);
        CHECK(dv->isColor());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isColor());
        CHECK(scriptedDataValue->dataValue()->as<DataValueColor>()->value() ==
              0x000000FF);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("another scripted color converter", "[scripting]")
{
    ScriptingTest vm(
        R"(type ColorConverter = {}
function convert(self: ColorConverter, input: DataValueColor): DataValueColor
  local dv: DataValueColor = DataValue.color()
  if input:isColor() then
    dv.alpha = input.red
    dv.red = input.green
    dv.green = input.blue
    dv.blue = input.alpha
  end
  return dv
end
function reverseConvert(
  self: ColorConverter,
  input: DataValueColor
): DataValueColor
  local dv: DataValueColor = DataValue.color()
  dv.value = input.value
  return dv
end
function init(self: ColorConverter): boolean
  return true
end
return function(): Converter<ColorConverter, DataValueColor, DataValueColor>
  return { convert = convert, reverseConvert = reverseConvert, init = init }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueColor>(L, L, 0x11223344);
        CHECK(dv->isColor());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isColor());
        CHECK(scriptedDataValue->dataValue()->as<DataValueColor>()->value() ==
              0x22334411);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
    {
        lua_getglobal(L, "convert");
        lua_pushvalue(L, -2);
        auto dv = lua_newrive<ScriptedDataValueBoolean>(L, L, true);
        CHECK(dv->isBoolean());

        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, -1);
        CHECK(scriptedDataValue->isColor());
        CHECK(scriptedDataValue->dataValue()->as<DataValueColor>()->value() ==
              0x00000000);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted converter survives Number<->String direction flips",
          "[scripting]")
{
    ScriptingTest vm(
        R"(type NumberToStringConverter = {}

function convert(self: NumberToStringConverter, input: DataValueNumber): DataValueString
  local result = DataValue.string()
  result.value = tostring(math.round(input.value))
  return result
end

function reverseConvert(self: NumberToStringConverter, input: DataValueString): DataValueNumber
  local result = DataValue.number()
  result.value = tonumber(input.value) or 0
  return result
end

return function(): Converter<NumberToStringConverter, DataValueNumber, DataValueString>
  return {
    convert = convert,
    reverseConvert = reverseConvert,
  }
end
)");

    // ScriptingTest leaves the factory function on top of the stack;
    // ensureScriptInitialized consumes it to build m_self.
    ScriptedDataConverter converter;
    // Enable convert() (bit 10) and reverseConvert() (bit 11).
    converter.implementedMethods((1 << 10) | (1 << 11));
    REQUIRE(converter.dataConverts());
    REQUIRE(converter.dataReverseConverts());
    REQUIRE(converter.ensureScriptInitialized(vm.vm()));

    // Flip directions repeatedly on the same converter instance. Each flip
    // changes the type storeData<T> must produce; the buggy version crashed
    // on the second call.
    for (int i = 0; i < 3; i++)
    {
        DataValueNumber number(20000 + i);
        auto forward = converter.convert(&number, nullptr);
        REQUIRE(forward != nullptr);
        REQUIRE(forward->is<DataValueString>());
        CHECK(forward->as<DataValueString>()->value() ==
              std::to_string(20000 + i));

        DataValueString text(std::to_string(12345 + i));
        auto reverse = converter.reverseConvert(&text, nullptr);
        REQUIRE(reverse != nullptr);
        REQUIRE(reverse->is<DataValueNumber>());
        CHECK(reverse->as<DataValueNumber>()->value() == (float)(12345 + i));
    }
}

TEST_CASE("scripted string converter", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/script_string_converter_test.riv", &silver);
    auto artboard = file->artboardNamed("Converter");

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

    auto field1 =
        vmi->propertyValue("Field1")->as<rive::ViewModelInstanceString>();
    REQUIRE(field1 != nullptr);
    field1->propertyValue("H#e%l&l*o");

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    auto field2 =
        vmi->propertyValue("Field2")->as<rive::ViewModelInstanceString>();
    REQUIRE(field2 != nullptr);
    field2->propertyValue("____one two three___");

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    auto field3 =
        vmi->propertyValue("Field3")->as<rive::ViewModelInstanceString>();
    REQUIRE(field3 != nullptr);
    field3->propertyValue("  **This uses a string converter@@. ");

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    auto field4 =
        vmi->propertyValue("Field4")->as<rive::ViewModelInstanceString>();
    REQUIRE(field4 != nullptr);
    field4->propertyValue("It strips special characters like *&^%$#@!)()");

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("script_string_converter"));
}

TEST_CASE("Data converter with bound inputs in artboard and state machine",
          "[silver]")
{
    SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/scripted_data_converter_bound_input.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    stateMachine->advanceAndApply(0.1f);

    artboard->draw(renderer.get());

    CHECK(silver.matches("scripted_data_converter_bound_input"));
}