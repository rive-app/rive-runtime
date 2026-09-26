/*
 * Copyright 2026 Rive
 */

// Instances cross the deferred boundary as a resource rather than as draw
// payload. They are created once, updated in place, and destroyed by handle.
// These tests cover that lifetime by performing one draw through record and
// replay, an edit between two frames, and the destroy recorded when the last
// reference goes.

#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/gpu_census.hpp"
#include "utils/serialized_replay.hpp"
#include "utils/no_op_renderer.hpp"
#include "deferred_test_sink.hpp"
#include "assets/nomoon.png.hpp"

#include <catch.hpp>

using namespace rive;
using deferred_test::TestSink;

namespace
{
constexpr size_t InstanceCount = 3;

// What the far side of the replay was asked to draw.
class MeshSink : public NoOpRenderer
{
public:
    int instancedDraws = 0;
    int plainDraws = 0;
    std::vector<std::vector<ImageMeshInstanceData>> frames;

    void drawImageMesh(const RenderImage*,
                       ImageSampler,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       uint32_t,
                       uint32_t,
                       BlendMode,
                       float) override
    {
        ++plainDraws;
    }

    void drawImageMeshInstanced(const RenderImage*,
                                ImageSampler,
                                rcp<RenderBuffer>,
                                rcp<RenderBuffer>,
                                rcp<RenderBuffer>,
                                uint32_t,
                                uint32_t,
                                rcp<ImageMeshInstances> instances) override
    {
        ++instancedDraws;
        frames.emplace_back(instances->instanceData().begin(),
                            instances->instanceData().end());
    }
};

// The mesh a session draws with. The geometry is beside the point and only
// the instances vary.
struct Mesh
{
    rcp<RenderImage> image;
    rcp<RenderBuffer> pts, uvs, indices;

    static Mesh make(Factory& factory)
    {
        Mesh mesh;
        mesh.image = factory.decodeImage(assets::nomoon_png());
        const Vec2D quad[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
        const Vec2D uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
        mesh.pts =
            buffer(factory, RenderBufferType::vertex, quad, sizeof(quad));
        mesh.uvs = buffer(factory, RenderBufferType::vertex, uv, sizeof(uv));
        mesh.indices =
            buffer(factory, RenderBufferType::index, idx, sizeof(idx));
        return mesh;
    }

    static rcp<RenderBuffer> buffer(Factory& factory,
                                    RenderBufferType type,
                                    const void* source,
                                    size_t size)
    {
        auto b = factory.makeRenderBuffer(type, RenderBufferFlags::none, size);
        memcpy(b->map(), source, size);
        b->unmap();
        return b;
    }

    void draw(Renderer* renderer, rcp<ImageMeshInstances> instances) const
    {
        renderer->drawImageMeshInstanced(image.get(),
                                         ImageSampler::LinearClamp(),
                                         pts,
                                         uvs,
                                         indices,
                                         4,
                                         6,
                                         std::move(instances));
    }
};

void replayFrame(cmd::DeferredSession& session,
                 cmd::DeferredReplayer& replayer,
                 TestSink& sink)
{
    cmd::DeferredFrame frame = cmd::takeFrame(session);
    replayer.replayFrame(frame, sink);
}

// The sink re-records into a SerializingFactory, so reading the instances back
// means replaying that stream once more.
void readBack(TestSink& sink, MeshSink& out)
{
    SerializingFactory discard;
    REQUIRE(replaySerializedCommands(sink.serializingFactory.bytes(),
                                     &discard,
                                     &out));
}
} // namespace

TEST_CASE("an instanced draw survives the deferred round trip",
          "[deferred][mesh]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    Mesh mesh = Mesh::make(session);
    auto instances = session.makeImageMeshInstances(InstanceCount);
    Span<ImageMeshInstanceData> data = instances->edit();
    data[0].transform = Mat2D(1, 0, 0, 1, 10, 20);
    data[1].opacity = 0.5f;
    data[2].uvScale = {0.5f, 0.25f};
    data[2].additiveness = 1.0f;
    instances->endEdit();

    mesh.draw(session.screenRenderer(), instances);
    replayFrame(session, replayer, sink);
    CHECK(replayer.droppedDraws() == 0);

    MeshSink out;
    readBack(sink, out);

    // One call, not a fan out, with every field where it was put.
    CHECK(out.instancedDraws == 1);
    CHECK(out.plainDraws == 0);
    REQUIRE(out.frames.size() == 1);
    REQUIRE(out.frames[0].size() == InstanceCount);
    CHECK(out.frames[0][0].transform[4] == 10.0f);
    CHECK(out.frames[0][0].transform[5] == 20.0f);
    CHECK(out.frames[0][1].opacity == 0.5f);
    CHECK(out.frames[0][2].uvScale.x == 0.5f);
    CHECK(out.frames[0][2].uvScale.y == 0.25f);
    CHECK(out.frames[0][2].additiveness == 1.0f);
}

// The resource keeps its handle across an edit, so the second frame has to
// pick up the new values rather than replaying the first frame's.
TEST_CASE("an in-place instance update reaches the next frame",
          "[deferred][mesh]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    Mesh mesh = Mesh::make(session);
    auto instances = session.makeImageMeshInstances(InstanceCount);
    instances->edit()[0].opacity = 0.25f;
    instances->endEdit();
    mesh.draw(session.screenRenderer(), instances);
    replayFrame(session, replayer, sink);

    instances->edit()[0].opacity = 0.75f;
    instances->endEdit();
    mesh.draw(session.screenRenderer(), instances);
    replayFrame(session, replayer, sink);

    MeshSink out;
    readBack(sink, out);

    REQUIRE(out.frames.size() == 2);
    CHECK(out.frames[0][0].opacity == 0.25f);
    CHECK(out.frames[1][0].opacity == 0.75f);
}

// Without a virtual ~ImageMeshInstances the deferred subclass never runs its
// destructor, so no destroy is ever recorded and the handle leaks for the
// life of the session.
TEST_CASE("dropping instances frees the replayer's copy", "[deferred][mesh]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    Mesh mesh = Mesh::make(session);
    auto instances = session.makeImageMeshInstances(InstanceCount);
    instances->edit();
    instances->endEdit();
    mesh.draw(session.screenRenderer(), instances);
    replayFrame(session, replayer, sink);
    REQUIRE(replayer.gpuCensus().imageMeshInstances == 1);

    // Destroys only enter the stream at a frame boundary, so an idle session
    // needs them drained by hand.
    instances = nullptr;
    auto pending = session.takePendingDestroys();
    REQUIRE_FALSE(pending.empty());
    replayer.replayDestroys(pending.commands, pending.oreCommands);
    CHECK(replayer.gpuCensus().imageMeshInstances == 0);
}
