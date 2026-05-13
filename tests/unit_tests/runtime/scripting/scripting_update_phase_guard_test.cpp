/*
 * Copyright 2026 Rive
 */

// Tests for the per-object update phase guard. When a ScriptedObject is inside
// its scriptUpdate() callback (m_inUpdatePhase == true), calls to
// markNeedsUpdate() are silently ignored.

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_drawable.hpp"

using namespace rive;

// Extends ScriptedDrawable to inherit its real markNeedsUpdate() but stubs
// addScriptedDirt so the test doesn't need a full artboard graph.
class TestScriptedDrawable : public ScriptedDrawable
{
    int m_dirtCount = 0;

public:
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        m_dirtCount++;
        return true;
    }

    int dirtCount() const { return m_dirtCount; }
    void resetDirtCount() { m_dirtCount = 0; }
    bool isInUpdatePhase() const { return inUpdatePhase(); }
};

TEST_CASE("markNeedsUpdate is ignored during scriptUpdate", "[scripting]")
{
    ScriptingTest vm(
        R"(
type MyObj = {
  _ctx: Context?,
}

function update(self: MyObj)
  if self._ctx then
    self._ctx:markNeedsUpdate()
  end
end

return function(): Node<MyObj>
  return {
    _ctx = nil,
    update = update,
  }
end
)");

    TestScriptedDrawable obj;
    lua_State* L = vm.state();

    CHECK(obj.ensureScriptInitialized(vm.vm()));

    // Attach the context to the Lua self table.
    rive_lua_pushRef(L, obj.self());
    auto* ctxPtr = lua_newrive<ScriptedContext>(L, &obj);
    lua_setfield(L, -2, "_ctx");
    rive_lua_pop(L, 1);

    // Mark the object as having an update method.
    obj.implementedMethods(obj.implementedMethods() | (1 << 1));
    CHECK(obj.updates());

    // scriptUpdate sets the phase flag, so markNeedsUpdate is suppressed.
    obj.resetDirtCount();
    CHECK(obj.isInUpdatePhase() == false);
    obj.scriptUpdate();
    CHECK(obj.isInUpdatePhase() == false);
    CHECK(obj.dirtCount() == 0);

    // Outside the update phase, markNeedsUpdate adds dirt normally.
    obj.resetDirtCount();
    obj.markNeedsUpdate();
    CHECK(obj.dirtCount() == 1);

    ctxPtr->clearScriptedObject();
}

TEST_CASE("inUpdatePhase defaults to false", "[scripting]")
{
    TestScriptedDrawable obj;
    CHECK(obj.isInUpdatePhase() == false);
}

TEST_CASE("markNeedsUpdate works outside update phase", "[scripting]")
{
    TestScriptedDrawable obj;
    CHECK(obj.dirtCount() == 0);
    obj.markNeedsUpdate();
    CHECK(obj.dirtCount() == 1);
    obj.markNeedsUpdate();
    CHECK(obj.dirtCount() == 2);
}
