/*
 * Copyright 2026 Rive
 */

// Tests for the canvasDrawingPhase gate.  The flag is set by
// `Artboard::drawCanvases()` via `ScopedCanvasDrawingPhase` and is checked by
// the Lua bindings that start canvas-level GPU work — `Canvas:beginFrame()`
// and `GPUCanvas:beginRenderPass()` — so a script can't begin a canvas draw
// from a non-draw callback (state-machine input handler, Coop event, async
// completion, …).
//
// `Canvas` and `GPUCanvas` are gated behind `RIVE_CANVAS` / `RIVE_ORE` build
// flags that aren't enabled in the unit-test build, so we can't exercise
// those bindings directly here.  Instead we cover the gate at the C++ level
// via the `ScopedCanvasDrawingPhase` RAII helper, and verify that the
// non-gated `Artboard:drawCanvas()` binding (which has no GPU dependency)
// continues to be callable from any phase.

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

TEST_CASE("ScopedCanvasDrawingPhase toggles the flag and restores it",
          "[scripting]")
{
    ScriptingTest vm("function noop():() end");
    lua_State* L = vm.state();
    auto* context = static_cast<ScriptingContext*>(lua_getthreaddata(L));
    REQUIRE(context != nullptr);

    // Default state: not in a drawing phase.
    CHECK(context->canvasDrawingPhase() == false);

    {
        ScopedCanvasDrawingPhase phase(context);
        CHECK(context->canvasDrawingPhase() == true);

        // Nested scopes preserve the previous (true) value when they unwind,
        // so reentrant draws don't accidentally clear the outer phase.
        {
            ScopedCanvasDrawingPhase nested(context);
            CHECK(context->canvasDrawingPhase() == true);
        }
        CHECK(context->canvasDrawingPhase() == true);
    }

    // Restored to the original false after the outer scope unwinds.
    CHECK(context->canvasDrawingPhase() == false);
}

TEST_CASE("ScopedCanvasDrawingPhase tolerates a null context", "[scripting]")
{
    // Some host code paths (e.g. early init / teardown) may not have a
    // ScriptingContext yet.  The RAII helper has to be a no-op in that case
    // rather than crash, since `Artboard::drawCanvases()` constructs it
    // unconditionally.
    ScopedCanvasDrawingPhase phase(nullptr);
    SUCCEED("ScopedCanvasDrawingPhase(nullptr) did not crash");
}

TEST_CASE("Artboard:drawCanvas() is callable regardless of drawing phase",
          "[scripting]")
{
    // `Artboard:drawCanvas()` itself is not gated — only the canvas-level GPU
    // entry points (`Canvas:beginFrame()`, `GPUCanvas:beginRenderPass()`) are.
    // Verify the binding succeeds both inside and outside the drawing phase.
    // `coin.riv` has no scripted objects so internalDrawCanvases() walks an
    // empty list and returns cleanly in either case.
    ScriptingTest vm("function callDrawCanvas(artboard:Artboard):()\n"
                     "  artboard:drawCanvas()\n"
                     "end\n");
    lua_State* L = vm.state();
    auto* context = static_cast<ScriptingContext*>(lua_getthreaddata(L));
    REQUIRE(context != nullptr);
    REQUIRE(context->canvasDrawingPhase() == false);

    auto file = ReadRiveFile("assets/coin.riv", vm.serializer());
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    lua_newrive<ScriptedArtboard>(L,
                                  L,
                                  file,
                                  artboard->instance(),
                                  nullptr,
                                  nullptr);

    // Outside the drawing phase: still succeeds.
    lua_getglobal(L, "callDrawCanvas");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);

    // Inside the drawing phase: also succeeds.
    {
        ScopedCanvasDrawingPhase phase(context);
        CHECK(context->canvasDrawingPhase() == true);
        lua_getglobal(L, "callDrawCanvas");
        lua_pushvalue(L, -2);
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
    }

    // Phase restored to false after the scope.
    CHECK(context->canvasDrawingPhase() == false);
}
