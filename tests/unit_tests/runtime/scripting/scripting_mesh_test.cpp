
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"

using namespace rive;

namespace
{
// Counts commits. A commit is where the deferred and serializing renderers
// copy the whole instance array, so it is the cost to keep down.
class CountingInstances : public ImageMeshInstances
{
public:
    CountingInstances(size_t count) : ImageMeshInstances(count) {}

    int commits = 0;

protected:
    void onEndEdit() override { ++commits; }
};

class CountingFactory : public NoOpFactory
{
public:
    rcp<CountingInstances> instances;

    rcp<ImageMeshInstances> makeImageMeshInstances(size_t count) override
    {
        instances = make_rcp<CountingInstances>(count);
        return instances;
    }
};

class TestImage : public RenderImage
{};

// Keeps the instances each instanced draw was handed.
class InstanceRecorder : public NoOpRenderer
{
public:
    std::vector<std::vector<ImageMeshInstanceData>> draws;
    bool drewWhileEditing = false;

    void drawImageMeshInstanced(const RenderImage*,
                                ImageSampler,
                                rcp<RenderBuffer>,
                                rcp<RenderBuffer>,
                                rcp<RenderBuffer>,
                                uint32_t,
                                uint32_t,
                                rcp<ImageMeshInstances> instances) override
    {
#ifdef DEBUG
        drewWhileEditing = drewWhileEditing || instances->isEditing();
#endif
        draws.emplace_back(instances->instanceData().begin(),
                           instances->instanceData().end());
    }
};

// One MeshInstances with fill(count, opacity) to write it and render(renderer,
// image) to draw it.
const char* meshInstancesSource =
    R"(local vertices = VertexBuffer()
vertices:add(Vector.xy(0, 0), Vector.xy(1, 0), Vector.xy(1, 1))
local uvs = VertexBuffer()
uvs:add(Vector.xy(0, 0), Vector.xy(1, 0), Vector.xy(1, 1))
local triangles = TriangleBuffer()
triangles:add(0, 1, 2)
local sampler = ImageSampler('clamp', 'clamp', 'bilinear')
local instances = MeshInstances()

function fill(count: number, opacity: number)
  instances:resize(count)
  for i = 0, count - 1 do
    instances:set(i, Mat2D.withTranslation(i, 0), opacity, 0.25, Vector.xy(0.5, 0), Vector.xy(0.5, 1))
  end
end

function grow(count: number)
  for i = 0, count - 1 do
    instances:resize(i + 1)
    instances:set(i, Mat2D.withTranslation(10 + i, 0))
  end
end

function setPastEnd(): string
  instances:resize(2)
  local ok, message = pcall(function()
    instances:set(2, Mat2D.identity())
  end)
  return message
end

function render(renderer: Renderer, image: Image)
  renderer:drawImageMeshInstanced(image, sampler, vertices, uvs, triangles, instances)
end
)";

void fill(lua_State* L, int count, float opacity)
{
    lua_getglobal(L, "fill");
    lua_pushnumber(L, count);
    lua_pushnumber(L, opacity);
    REQUIRE(lua_pcall(L, 2, 0, 0) == LUA_OK);
}

void render(lua_State* L, Renderer* renderer)
{
    lua_getglobal(L, "render");
    auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, renderer);
    ScriptedImage::luaNew(L)->image = make_rcp<TestImage>();
    REQUIRE(lua_pcall(L, 2, 0, 0) == LUA_OK);
    CHECK(scriptedRenderer->end());
}
} // namespace

TEST_CASE("MeshInstances:set commits once per draw", "[scripting]")
{
    CountingFactory factory;
    ScriptingTest vm(meshInstancesSource, 0, false, {}, true, &factory);
    lua_State* L = vm.state();
    InstanceRecorder recorder;
    REQUIRE(factory.instances != nullptr);

    // A hundred set() calls hand the renderer nothing until the draw, which
    // takes them all in one commit.
    fill(L, 100, 0.5f);
    CHECK(factory.instances->commits == 0);
    render(L, &recorder);
    CHECK(factory.instances->commits == 1);
    CHECK(!recorder.drewWhileEditing);

    REQUIRE(recorder.draws.size() == 1);
    REQUIRE(recorder.draws[0].size() == 100);
    const ImageMeshInstanceData& seventh = recorder.draws[0][7];
    CHECK(seventh.transform.tx() == 7.0f);
    CHECK(seventh.opacity == 0.5f);
    CHECK(seventh.additiveness == 0.25f);
    CHECK(seventh.uvTranslate == Vec2D(0.5f, 0.0f));
    CHECK(seventh.uvScale == Vec2D(0.5f, 1.0f));

    // The next frame rewrites every instance, and still commits once.
    fill(L, 100, 0.75f);
    render(L, &recorder);
    CHECK(factory.instances->commits == 2);
    REQUIRE(recorder.draws.size() == 2);
    CHECK(recorder.draws[1][99].opacity == 0.75f);

    // Drawing again without writing commits nothing.
    render(L, &recorder);
    CHECK(factory.instances->commits == 2);
    REQUIRE(recorder.draws.size() == 3);
    CHECK(recorder.draws[2][99].opacity == 0.75f);
}

TEST_CASE("MeshInstances:resize keeps earlier sets and commits once per draw",
          "[scripting]")
{
    CountingFactory factory;
    ScriptingTest vm(meshInstancesSource, 0, false, {}, true, &factory);
    lua_State* L = vm.state();
    InstanceRecorder recorder;
    REQUIRE(factory.instances != nullptr);

    // Growing one instance at a time resizes before every set(). None of
    // those resizes may commit, or growth costs a copy of the array per step.
    lua_getglobal(L, "grow");
    lua_pushnumber(L, 100);
    REQUIRE(lua_pcall(L, 1, 0, 0) == LUA_OK);
    CHECK(factory.instances->commits == 0);
    render(L, &recorder);
    CHECK(factory.instances->commits == 1);
    CHECK(!recorder.drewWhileEditing);

    REQUIRE(recorder.draws.size() == 1);
    REQUIRE(recorder.draws[0].size() == 100);
    for (size_t i = 0; i < 100; i++)
    {
        CHECK(recorder.draws[0][i].transform.tx() == 10.0f + i);
    }
}

TEST_CASE("MeshInstances:set rejects an index past the end", "[scripting]")
{
    CountingFactory factory;
    ScriptingTest vm(meshInstancesSource, 0, false, {}, true, &factory);
    lua_State* L = vm.state();

    lua_getglobal(L, "setPastEnd");
    REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
    REQUIRE(lua_isstring(L, -1));
    CHECK(std::string(lua_tostring(L, -1))
              .find("index 2 is past the end of MeshInstances") !=
          std::string::npos);
}

TEST_CASE("mesh can be constructed", "[scripting]")
{
    CHECK(
        lua_userdatatag(
            ScriptingTest(
                // clang-format off
                            "local _vertices = VertexBuffer()\n"
                            "_vertices:add(Vector.xy(0,0),Vector.xy(1,0),Vector.xy(1,1))\n"
                            "local _uvs = VertexBuffer()\n"
                            "_uvs:add(Vector.xy(0,0))\n"
                            "_uvs:add(Vector.xy(1,0))\n"
                            "_uvs:add(Vector.xy(1,1))\n"
                            "local indices = TriangleBuffer()\n"
                            "indices:add(0, 1, 2)\n"
                            "return indices"
                // clang-format on
                )
                .state(),
            -1) == ScriptedTriangleBuffer::luaTag);
}

TEST_CASE("ImageSampler can be created", "[scripting]")
{
    CHECK(
        lua_userdatatag(ScriptingTest(
                            // clang-format off
                            "return ImageSampler('clamp', 'mirror', 'nearest')"
                            // clang-format on
                            )
                            .state(),
                        -1) == ScriptedImageSampler::luaTag);
}
