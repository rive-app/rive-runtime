#include <catch.hpp>
#include "scripting_test_utilities.hpp"
#include <algorithm>
#include <chrono>
#include <vector>

using namespace rive;

// Every ScriptedProperty is tracked on its owner's list, or on the context's
// orphan list when nothing owns it, and untracks itself when disposed.
// Teardown disposes every one of them, so untracking has to be O(1): a script
// that resolved a few hundred properties per frame left hundreds of thousands
// on the editor's orphan list, and scanning for each one on teardown froze the
// editor for minutes.
namespace
{
// Creates `count` number properties, kept alive by a table left on the stack.
std::vector<ScriptedProperty*> makeProperties(lua_State* L, int count)
{
    std::vector<ScriptedProperty*> properties;
    properties.reserve(count);
    lua_createtable(L, count, 0);
    for (int i = 1; i <= count; i++)
    {
        properties.push_back(
            lua_newrive<ScriptedPropertyNumber>(L, L, nullptr));
        lua_rawseti(L, -2, i);
    }
    return properties;
}

ScriptingContext* contextOf(lua_State* L)
{
    return static_cast<ScriptingContext*>(lua_getthreaddata(L));
}
} // namespace

TEST_CASE("untracking in any order keeps an object's tracked list intact",
          "[scripting_property_tracking]")
{
    ScriptingTest test("return 0");
    lua_State* L = test.state();
    ScriptedObjectTest object;

    std::vector<ScriptedProperty*> properties;
    {
        ScopedScriptedObjectContext scope(contextOf(L), &object);
        properties = makeProperties(L, 2000);
    }
    REQUIRE(object.trackedScriptedProperties().size() == 2000);

    // Dispose every third property back to front, then every fifth front to
    // back, so removals hit the middle, the end and already-moved slots.
    std::vector<ScriptedProperty*> remaining;
    for (int i = (int)properties.size() - 1; i >= 0; i--)
    {
        if (i % 3 == 0)
        {
            properties[i]->dispose();
        }
    }
    for (size_t i = 0; i < properties.size(); i++)
    {
        if (i % 5 == 0)
        {
            properties[i]->dispose();
        }
        if (!properties[i]->disposed())
        {
            remaining.push_back(properties[i]);
        }
    }

    const auto& tracked = object.trackedScriptedProperties();
    REQUIRE(tracked.size() == remaining.size());
    for (size_t i = 0; i < tracked.size(); i++)
    {
        CHECK(tracked[i]->trackIndex() == i);
        CHECK(tracked[i]->owner() == &object);
    }
    std::vector<ScriptedProperty*> sorted(tracked.begin(), tracked.end());
    std::sort(sorted.begin(), sorted.end());
    std::sort(remaining.begin(), remaining.end());
    CHECK(sorted == remaining);

    object.scriptDispose();
    for (auto* property : properties)
    {
        CHECK(property->disposed());
    }
    lua_pop(L, 1);
}

TEST_CASE("the orphan sweep reaches every orphan after scattered untracking",
          "[scripting_property_tracking]")
{
    ScriptingTest test("return 0");
    lua_State* L = test.state();

    auto properties = makeProperties(L, 2000);
    for (size_t i = 0; i < properties.size(); i += 2)
    {
        properties[i]->dispose();
    }
    contextOf(L)->disposeOrphanScriptedProperties(/*allTags=*/true);
    for (auto* property : properties)
    {
        CHECK(property->disposed());
    }
    lua_pop(L, 1);
}

TEST_CASE("disposing many orphans is linear", "[scripting_property_tracking]")
{
    ScriptingTest test("return 0");
    lua_State* L = test.state();

    // Quadratic untracking needs ~2e10 scans here and takes minutes; linear
    // finishes in milliseconds even in debug and sanitizer builds.
    auto properties = makeProperties(L, 200000);
    auto start = std::chrono::steady_clock::now();
    contextOf(L)->disposeOrphanScriptedProperties(/*allTags=*/true);
    auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(properties.front()->disposed());
    CHECK(properties.back()->disposed());
    CHECK(elapsed < std::chrono::seconds(2));
    lua_pop(L, 1);
}
