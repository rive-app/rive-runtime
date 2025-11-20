
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/math/path_types.hpp"

using namespace rive;

TEST_CASE("path contours returns first contour", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "path:close()\n"
                     "local contour = path:contours()\n"
                     "return contour ~= nil\n");
    lua_State* L = vm.state();
    CHECK(lua_toboolean(L, -1));
}

TEST_CASE("contour measure has length", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "path:close()\n"
                     "local contour = path:contours()\n"
                     "return contour.length\n");
    lua_State* L = vm.state();
    float length = (float)lua_tonumber(L, -1);
    CHECK(length > 0.0f);
    CHECK(length < 100.0f); // Should be around 30 for a 10x10 rectangle
}

TEST_CASE("contour measure isClosed", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "path:close()\n"
                     "local contour = path:contours()\n"
                     "return contour.isClosed\n");
    lua_State* L = vm.state();
    CHECK(lua_toboolean(L, -1));
}

TEST_CASE("contour measure isClosed false for open path", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "local contour = path:contours()\n"
                     "return contour.isClosed\n");
    lua_State* L = vm.state();
    CHECK(!lua_toboolean(L, -1));
}

TEST_CASE("contour measure positionAndTangent", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "local contour = path:contours()\n"
                     "local pos, tan = contour:positionAndTangent(0)\n"
                     "return pos.x, pos.y, tan.x, tan.y\n",
                     4);
    lua_State* L = vm.state();
    float posX = (float)lua_tonumber(L, -4);
    float posY = (float)lua_tonumber(L, -3);
    float tanX = (float)lua_tonumber(L, -2);
    float tanY = (float)lua_tonumber(L, -1);
    CHECK(posX == Approx(0.0f));
    CHECK(posY == Approx(0.0f));
    CHECK(tanX > 0.0f); // Tangent should point along the line
    CHECK(tanY == Approx(0.0f));
}

TEST_CASE("contour measure warp", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "local contour = path:contours()\n"
                     "local result = contour:warp(Vector.xy(5, 2))\n"
                     "return result.x, result.y\n",
                     2);
    lua_State* L = vm.state();
    float x = (float)lua_tonumber(L, -2);
    float y = (float)lua_tonumber(L, -1);
    CHECK(x == Approx(5.0f));
    CHECK(y == Approx(2.0f)); // Offset perpendicular to path
}

TEST_CASE("contour measure extract", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "local contour = path:contours()\n"
                     "local destPath: Path = Path.new()\n"
                     "contour:extract(0, 10, destPath, true)\n"
                     "return destPath\n");
    lua_State* L = vm.state();
    auto destPath = lua_torive<ScriptedPath>(L, -1);
    CHECK(destPath != nullptr);
    // The extracted path should have some content
    CHECK(destPath->rawPath.verbs().count() > 0);
}

TEST_CASE("contour measure extract defaults to startWithMove true",
          "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "local contour = path:contours()\n"
                     "local destPath: Path = Path.new()\n"
                     "contour:extract(0, 10, destPath)\n"
                     "return destPath\n");
    lua_State* L = vm.state();
    auto destPath = lua_torive<ScriptedPath>(L, -1);
    CHECK(destPath != nullptr);
    CHECK(destPath->rawPath.verbs().count() > 0);
    // When startWithMove is not provided, it should default to true
    // So the first verb should be a move
    CHECK(destPath->rawPath.verbs()[0] == PathVerb::move);
}

TEST_CASE("contour measure extract with startWithMove false", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "local contour = path:contours()\n"
                     "local destPath: Path = Path.new()\n"
                     "destPath:moveTo(Vector.xy(100, 100))\n"
                     "contour:extract(0, 10, destPath, false)\n"
                     "return destPath\n");
    lua_State* L = vm.state();
    auto destPath = lua_torive<ScriptedPath>(L, -1);
    CHECK(destPath != nullptr);
    CHECK(destPath->rawPath.verbs().count() > 1);
    // When startWithMove is false and path already has a move,
    // the extracted segment should continue without adding another move
    // So we should have the initial move, then the extracted geometry
    CHECK(destPath->rawPath.verbs()[0] == PathVerb::move);
    // The second verb should be a line (from the extracted segment)
    CHECK(destPath->rawPath.verbs()[1] == PathVerb::line);
}

TEST_CASE("contour measure next iterates contours", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:close()\n"
                     "path:moveTo(Vector.xy(20, 20))\n"
                     "path:lineTo(Vector.xy(30, 20))\n"
                     "path:close()\n"
                     "local contour1 = path:contours()\n"
                     "local contour2 = contour1.next\n"
                     "return contour1 ~= nil, contour2 ~= nil\n",
                     2);
    lua_State* L = vm.state();
    CHECK(lua_toboolean(L, -2)); // First contour exists
    CHECK(lua_toboolean(L, -1)); // Second contour exists
}

TEST_CASE("contour measure next returns nil when done", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:close()\n"
                     "local contour1 = path:contours()\n"
                     "local contour2 = contour1.next\n"
                     "return contour2 == nil\n");
    lua_State* L = vm.state();
    CHECK(lua_toboolean(L, -1));
}

TEST_CASE("path measure returns PathMeasure", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "path:close()\n"
                     "local measure = path:measure()\n"
                     "return measure ~= nil\n");
    lua_State* L = vm.state();
    CHECK(lua_toboolean(L, -1));
}

TEST_CASE("path measure has length", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "path:close()\n"
                     "local measure = path:measure()\n"
                     "return measure.length\n");
    lua_State* L = vm.state();
    float length = (float)lua_tonumber(L, -1);
    CHECK(length > 0.0f);
    CHECK(length < 100.0f);
}

TEST_CASE("path measure isClosed for single closed contour", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "path:close()\n"
                     "local measure = path:measure()\n"
                     "return measure.isClosed\n");
    lua_State* L = vm.state();
    CHECK(lua_toboolean(L, -1));
}

TEST_CASE("path measure isClosed false for multiple contours", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:close()\n"
                     "path:moveTo(Vector.xy(20, 20))\n"
                     "path:lineTo(Vector.xy(30, 20))\n"
                     "path:close()\n"
                     "local measure = path:measure()\n"
                     "return measure.isClosed\n");
    lua_State* L = vm.state();
    CHECK(!lua_toboolean(L, -1));
}

TEST_CASE("path measure isClosed false for open path", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "local measure = path:measure()\n"
                     "return measure.isClosed\n");
    lua_State* L = vm.state();
    CHECK(!lua_toboolean(L, -1));
}

TEST_CASE("path measure positionAndTangent", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "local measure = path:measure()\n"
                     "local pos, tan = measure:positionAndTangent(0)\n"
                     "return pos.x, pos.y, tan.x, tan.y\n",
                     4);
    lua_State* L = vm.state();
    float posX = (float)lua_tonumber(L, -4);
    float posY = (float)lua_tonumber(L, -3);
    float tanX = (float)lua_tonumber(L, -2);
    float tanY = (float)lua_tonumber(L, -1);
    CHECK(posX == Approx(0.0f));
    CHECK(posY == Approx(0.0f));
    CHECK(tanX > 0.0f);
    CHECK(tanY == Approx(0.0f));
}

TEST_CASE("path measure warp", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "local measure = path:measure()\n"
                     "local result = measure:warp(Vector.xy(5, 2))\n"
                     "return result.x, result.y\n",
                     2);
    lua_State* L = vm.state();
    float x = (float)lua_tonumber(L, -2);
    float y = (float)lua_tonumber(L, -1);
    CHECK(x == Approx(5.0f));
    CHECK(y == Approx(2.0f));
}

TEST_CASE("path measure extract", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "local measure = path:measure()\n"
                     "local destPath: Path = Path.new()\n"
                     "measure:extract(0, 10, destPath, true)\n"
                     "return destPath\n");
    lua_State* L = vm.state();
    auto destPath = lua_torive<ScriptedPath>(L, -1);
    CHECK(destPath != nullptr);
    CHECK(destPath->rawPath.verbs().count() > 0);
}

TEST_CASE("path measure extract defaults to startWithMove true", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "local measure = path:measure()\n"
                     "local destPath: Path = Path.new()\n"
                     "measure:extract(0, 10, destPath)\n"
                     "return destPath\n");
    lua_State* L = vm.state();
    auto destPath = lua_torive<ScriptedPath>(L, -1);
    CHECK(destPath != nullptr);
    CHECK(destPath->rawPath.verbs().count() > 0);
    // When startWithMove is not provided, it should default to true
    // So the first verb should be a move
    CHECK(destPath->rawPath.verbs()[0] == PathVerb::move);
}

TEST_CASE("path measure extract with startWithMove false", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:lineTo(Vector.xy(10, 10))\n"
                     "local measure = path:measure()\n"
                     "local destPath: Path = Path.new()\n"
                     "destPath:moveTo(Vector.xy(100, 100))\n"
                     "measure:extract(0, 10, destPath, false)\n"
                     "return destPath\n");
    lua_State* L = vm.state();
    auto destPath = lua_torive<ScriptedPath>(L, -1);
    CHECK(destPath != nullptr);
    CHECK(destPath->rawPath.verbs().count() > 1);
    // When startWithMove is false and path already has a move,
    // the extracted segment should continue without adding another move
    // So we should have the initial move, then the extracted geometry
    CHECK(destPath->rawPath.verbs()[0] == PathVerb::move);
    // The second verb should be a line (from the extracted segment)
    CHECK(destPath->rawPath.verbs()[1] == PathVerb::line);
}

TEST_CASE("path measure extract across multiple contours", "[scripting]")
{
    ScriptingTest vm("local path: Path = Path.new()\n"
                     "path:moveTo(Vector.xy(0, 0))\n"
                     "path:lineTo(Vector.xy(10, 0))\n"
                     "path:close()\n"
                     "path:moveTo(Vector.xy(20, 0))\n"
                     "path:lineTo(Vector.xy(30, 0))\n"
                     "path:close()\n"
                     "local measure = path:measure()\n"
                     "local destPath: Path = Path.new()\n"
                     "measure:extract(5, 25, destPath, true)\n"
                     "return destPath\n");
    lua_State* L = vm.state();
    auto destPath = lua_torive<ScriptedPath>(L, -1);
    CHECK(destPath != nullptr);
    // Should extract from both contours
    CHECK(destPath->rawPath.verbs().count() > 0);
}
