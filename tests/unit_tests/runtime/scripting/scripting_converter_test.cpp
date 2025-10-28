
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"

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
