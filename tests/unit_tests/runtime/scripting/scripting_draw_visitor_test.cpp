#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include <algorithm>

using namespace rive;

namespace
{
int callWithArtboard(ScriptingTest& vm,
                     File* file,
                     Renderer* renderer,
                     const char* function)
{
    lua_State* L = vm.state();
    lua_getglobal(L, function);
    lua_newrive<ScriptedArtboard>(L,
                                  L,
                                  file,
                                  file->artboard()->instance(),
                                  nullptr,
                                  nullptr);
    auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, renderer);
    int status = lua_pcall(L, 2, 0, 0);
    scriptedRenderer->end();
    return status;
}

double globalNumber(lua_State* L, const char* name)
{
    lua_getglobal(L, name);
    double value = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return value;
}
} // namespace

TEST_CASE("artboard draw visitor reads custom properties", "[scripting]")
{
    ScriptingTest vm(R"(
visits = 0
emissive = 0
glows = 0
tint = 0
label = ""
missing = 0
kinds = ""
passed = 0
stored = nil
function render(artboard: Artboard, renderer: Renderer)
    artboard:advance(0)
    local EMISSIVE = artboard:propertyKey("emissive")
    local GLOW = artboard:propertyKey("glow")
    local TINT = artboard:propertyKey("tint")
    local LABEL = artboard:propertyKey("label")
    local NOPE = artboard:propertyKey("nope")
    renderer:save()
    renderer:modulateColor(Color.rgba(0, 0, 0, 255))
    artboard:draw(renderer, function(context, drawable, r)
        visits += 1
        passed = context
        stored = drawable
        emissive += drawable:number(EMISSIVE) or 0
        if drawable:boolean(GLOW) then
            glows += 1
            tint = drawable:color(TINT) or 0
            label = drawable:string(LABEL) or ""
            for _, property in drawable:properties() do
                kinds ..= property.name .. ":" .. property.kind .. " "
            end
        end
        if drawable:number(LABEL) == nil and drawable:number(NOPE) == nil then
            missing += 1
        end
        r:setColorModulation(Color.rgba(255, 255, 255, 255))
        drawable:draw(r)
    end, 7)
    renderer:restore()
end
function afterwards()
    stored:number(0)
end
function modulated(artboard: Artboard, renderer: Renderer)
    artboard:advance(0)
    renderer:save()
    renderer:setColorModulation(Color.rgba(0, 0, 0, 255))
    artboard:drawModulated(renderer, artboard:propertyKey("emissive"))
    renderer:restore()
end
function passing(artboard: Artboard, renderer: Renderer)
    artboard:advance(0)
    artboard:draw(renderer, function(context, drawable, r)
        drawable:draw(r)
    end)
end
function failingOpen(artboard: Artboard, renderer: Renderer)
    artboard:advance(0)
    artboard:draw(renderer, function(context, drawable, r)
        r:save()
        error("visitor failed with a save open")
    end)
end
function failing(artboard: Artboard, renderer: Renderer)
    artboard:advance(0)
    artboard:draw(renderer, function()
        error("visitor failed")
    end)
end
)");
    lua_State* L = vm.state();
    auto file =
        ReadRiveFile("assets/drawable_custom_properties.riv", vm.serializer());
    REQUIRE(file->artboard() != nullptr);
    auto renderer = vm.serializer()->makeRenderer();

    REQUIRE(callWithArtboard(vm, file.get(), renderer.get(), "render") ==
            LUA_OK);
    CHECK(globalNumber(L, "visits") == 4);
    CHECK(globalNumber(L, "emissive") == Approx(2.3));
    CHECK(globalNumber(L, "glows") == 1);
    CHECK(globalNumber(L, "tint") == 0xFF8040FF);
    CHECK(globalNumber(L, "missing") == 4);
    CHECK(globalNumber(L, "passed") == 7);
    lua_getglobal(L, "label");
    CHECK(std::string(lua_tostring(L, -1)) == "ghost");
    lua_getglobal(L, "kinds");
    CHECK(std::string(lua_tostring(L, -1)) ==
          "emissive:number glow:boolean tint:color label:string ");
    lua_pop(L, 2);

    // The drawable is borrowed for the visit only.
    lua_getglobal(L, "afterwards");
    CHECK(lua_pcall(L, 0, 0, 0) == LUA_ERRRUN);
    lua_pop(L, 1);

    CHECK(callWithArtboard(vm, file.get(), renderer.get(), "modulated") ==
          LUA_OK);

    // A visitor that fails with a save of its own open does not leave the
    // rest of the frame drawing one level deeper than a passing visitor does,
    // and only the drawable it failed on goes undrawn.
    {
        struct DepthRecorder : public NoOpRenderer
        {
            int depth = 0;
            int deepestDraw = 0;
            int draws = 0;
            void save() override { depth++; }
            void restore() override { depth--; }
            void drawPath(RenderPath*, RenderPaint*) override
            {
                draws++;
                deepestDraw = std::max(deepestDraw, depth);
            }
        };
        DepthRecorder passing;
        REQUIRE(callWithArtboard(vm, file.get(), &passing, "passing") ==
                LUA_OK);
        REQUIRE(passing.deepestDraw > 0);

        DepthRecorder failed;
        CHECK(callWithArtboard(vm, file.get(), &failed, "failingOpen") ==
              LUA_ERRRUN);
        lua_pop(L, 1);
        CHECK(failed.deepestDraw == passing.deepestDraw);
        CHECK(failed.draws == passing.draws - 1);
    }

    // A failing visitor surfaces once the draw has unwound.
    CHECK(callWithArtboard(vm, file.get(), renderer.get(), "failing") ==
          LUA_ERRRUN);
    CHECK(std::string(lua_tostring(L, -1)).find("visitor failed") !=
          std::string::npos);
}
