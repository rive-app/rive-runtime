
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/async/work_pool.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"

#include <cstring>

using namespace rive;

// Defined in lua_image_decode.cpp.
extern int context_decodeImage_impl(lua_State* L);
extern int context_loadImage_impl(lua_State* L);

// Wrapper so we can register context_decodeImage_impl as a plain Lua global.
// Shifts args so that arg 1 (the buffer) becomes arg 2, matching the
// context:decodeImage(buf) calling convention.
static int decodeImage_wrapper(lua_State* L)
{
  // context_decodeImage_impl expects self at 1, buffer at 2.
  // Push a dummy nil for self, then move the buffer after it.
  lua_pushnil(L);
  lua_insert(L, 1); // nil is now at index 1, buffer at index 2
  return context_decodeImage_impl(L);
}

// Same shift for context:loadImage(buf).
static int loadImage_wrapper(lua_State* L)
{
  lua_pushnil(L);
  lua_insert(L, 1);
  return context_loadImage_impl(L);
}

// Registers loadImage and a `bytes` buffer holding the file at path.
static void pushLoadImageGlobals(lua_State* L, const char* path)
{
  lua_pushcfunction(L, loadImage_wrapper, "loadImage");
  lua_setglobal(L, "loadImage");
  auto bytes = ReadFile(path);
  REQUIRE(!bytes.empty());
  memcpy(lua_newbuffer(L, bytes.size()), bytes.data(), bytes.size());
  lua_setglobal(L, "bytes");
}

// ============================================================================
// decodeImage cancellation
// ============================================================================

TEST_CASE(
    "decodeImage cancel sets promise to Cancelled",
    "[scripting][image_decode]")
{
  ScriptingTest test(
      R"(
        local p = decodeImage(buffer.create(4))
        p:cancel()
        return p:getStatus()
    )",
      1,
      false,
      {},
      false);

  // Register the wrapper before executing.
  lua_pushcfunction(test.state(), decodeImage_wrapper, "decodeImage");
  lua_setglobal(test.state(), "decodeImage");
  test.execute();

  CHECK(std::string(lua_tostring(test.state(), -1)) == "Cancelled");
}

TEST_CASE(
    "decodeImage cancel does not fire andThen",
    "[scripting][image_decode]")
{
  ScriptingTest test(
      R"(
        local called = false
        local p = decodeImage(buffer.create(4))
        p:andThen(function() called = true end)
        p:cancel()
        return called
    )",
      1,
      false,
      {},
      false);

  lua_pushcfunction(test.state(), decodeImage_wrapper, "decodeImage");
  lua_setglobal(test.state(), "decodeImage");
  test.execute();

  CHECK(lua_toboolean(test.state(), -1) == 0);
}

TEST_CASE("decodeImage cancel fires onCancel hook", "[scripting][image_decode]")
{
  ScriptingTest test(
      R"(
        local hookFired = false
        local p = decodeImage(buffer.create(4))
        p:onCancel(function() hookFired = true end)
        p:cancel()
        return hookFired
    )",
      1,
      false,
      {},
      false);

  lua_pushcfunction(test.state(), decodeImage_wrapper, "decodeImage");
  lua_setglobal(test.state(), "decodeImage");
  test.execute();

  CHECK(lua_toboolean(test.state(), -1) != 0);
}

// ============================================================================
// loadImage
// ============================================================================

TEST_CASE(
    "loadImage resolves with an Image the size of the encoded one",
    "[scripting][image_decode]")
{
  ScriptingTest test(
      R"(
        local p = loadImage(bytes)
        local w, h = 0, 0
        p:andThen(function(image)
          w = image.width
          h = image.height
        end)
        return p:getStatus() .. " " .. w .. "x" .. h
    )",
      1,
      false,
      {},
      false);

  pushLoadImageGlobals(test.state(), "assets/placeholder.png");
  test.execute();

  CHECK(std::string(lua_tostring(test.state(), -1)) == "Fulfilled 226x128");
}

TEST_CASE(
    "loadImage rejects bytes that are not an image",
    "[scripting][image_decode]")
{
  ScriptingTest test(
      R"(
        local reason = ""
        local p = loadImage(buffer.fromstring("not an image"))
        p:catch(function(err) reason = err end)
        return p:getStatus() .. ": " .. reason
    )",
      1,
      false,
      {},
      false);

  pushLoadImageGlobals(test.state(), "assets/placeholder.png");
  test.execute();

  CHECK(
      std::string(lua_tostring(test.state(), -1)) ==
      "Rejected: failed to decode image data");
}

TEST_CASE("loadImage errors on an empty buffer", "[scripting][image_decode]")
{
  ScriptingTest test(
      R"(
        loadImage(buffer.create(0))
    )",
      0,
      true,
      {},
      false);

  pushLoadImageGlobals(test.state(), "assets/placeholder.png");
  auto context = static_cast<TestScriptingContext*>(test.vm()->context());
  CHECK(context->pCall(test.state(), 0, 0) != LUA_OK);
  CHECK(
      std::string(lua_tostring(test.state(), -1))
          .find("loadImage: empty buffer") != std::string::npos);
}

TEST_CASE(
    "a loaded Image assigned to a view model image property reaches it",
    "[scripting][image_decode]")
{
  auto vmiImage = make_rcp<ViewModelInstanceAssetImage>();

  ScriptingTest test(
      R"(
        loadImage(bytes):andThen(function(image)
          imageProp.value = image
        end)
        return imageProp.value ~= nil and imageProp.value.width or 0
    )",
      1,
      false,
      {},
      false);

  pushLoadImageGlobals(test.state(), "assets/placeholder.png");
  lua_newrive<ScriptedPropertyImage>(test.state(), test.state(), vmiImage);
  lua_setglobal(test.state(), "imageProp");
  test.execute();

  CHECK(lua_tonumber(test.state(), -1) == 226);
  auto renderImage = vmiImage->asset()->renderImage();
  REQUIRE(renderImage != nullptr);
  CHECK(renderImage->width() == 226);
  CHECK(renderImage->height() == 128);
}
