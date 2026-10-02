/*
 * Copyright 2026 Rive
 */

// Replays a SerializingFactory stream into a second SerializingFactory and
// asserts the re-recorded stream is byte identical, proving every call is
// reproduced in order. GPU free, pixels are covered by the GMs.

#include "utils/serializing_factory.hpp"
#include "utils/serialized_replay.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/mat2d.hpp"
#include "rive_file_reader.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "utils/no_op_renderer.hpp"

#include <catch.hpp>
#include <cstring>

using namespace rive;

TEST_CASE("serialized 2D commands replay byte-identically",
          "[serialize][replay]")
{
    SerializingFactory a;
    a.frameSize(256, 256);
    a.addFrame();
    auto rendererA = a.makeRenderer();

    // Exercises every paint mutation op.
    auto paint = a.makeRenderPaint();
    paint->color(0xFF112233);
    paint->style(RenderPaintStyle::stroke);
    paint->thickness(3.5f);
    paint->join(StrokeJoin::round);
    paint->cap(StrokeCap::square);
    paint->blendMode(BlendMode::multiply);
    paint->feather(2.0f);
    paint->additiveness(0.375f);

    RawPath rp;
    rp.move({0, 0});
    rp.line({10, 0});
    rp.cubic({10, 5}, {5, 10}, {0, 10});
    rp.close();
    auto path = a.makeRenderPath(rp, FillRule::evenOdd);

    auto clip = a.makeEmptyRenderPath();
    RawPath cp;
    cp.move({0, 0});
    cp.line({20, 0});
    cp.line({20, 20});
    cp.close();
    clip->addRawPath(cp);

    ColorInt cols[2] = {0xFFFF0000, 0xFF0000FF};
    float stops[2] = {0.0f, 1.0f};
    auto grad = a.makeLinearGradient(0, 0, 100, 100, cols, stops, 2);
    auto paint2 = a.makeRenderPaint();
    paint2->shader(grad);

    // Modulating image: non-default sampler on every axis and an asymmetric
    // matrix, so a mis-ordered read/write of either shows up in the diff.
    auto imageBytes = ReadFile("assets/open_source.jpg");
    auto image = a.decodeImage(imageBytes);
    REQUIRE(image != nullptr);
    ImageSampler sampler;
    sampler.filter = ImageFilter::nearest;
    sampler.wrapX = ImageWrap::repeat;
    sampler.wrapY = ImageWrap::mirror;
    paint2->modulatedImage(image.get(), sampler, Mat2D(2, 3, 4, 5, 6, 7));

    // ...and the clear (null image) path, which writes image id 0.
    auto paint3 = a.makeRenderPaint();
    paint3->modulatedImage(image.get(), sampler, Mat2D(1, 0, 0, 1, 0, 0));
    paint3->modulatedImage(nullptr, ImageSampler::LinearClamp(), Mat2D());

    rendererA->save();
    rendererA->transform(Mat2D(1, 0, 0, 1, 5, 7));
    rendererA->clipPath(clip.get());
    rendererA->modulateOpacity(0.5f);
    rendererA->drawPath(path.get(), paint.get());
    rendererA->drawPath(path.get(), paint2.get());
    rendererA->drawPath(path.get(), paint3.get());

    // Images carry additiveness per draw rather than on a paint, and a non
    // zero value records as a different op, so both branches need covering.
    rendererA->drawImage(image.get(),
                         ImageSampler::LinearClamp(),
                         BlendMode::srcOver,
                         1.0f);
    rendererA->drawImage(image.get(),
                         ImageSampler::LinearClamp(),
                         BlendMode::srcOver,
                         1.0f,
                         0.625f);

    float verts[8] = {0, 0, 10, 0, 10, 10, 0, 10};
    float uvs[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
    auto vertexBuffer = a.makeRenderBuffer(RenderBufferType::vertex,
                                           RenderBufferFlags::none,
                                           sizeof(verts));
    auto uvBuffer = a.makeRenderBuffer(RenderBufferType::vertex,
                                       RenderBufferFlags::none,
                                       sizeof(uvs));
    auto indexBuffer = a.makeRenderBuffer(RenderBufferType::index,
                                          RenderBufferFlags::none,
                                          sizeof(indices));
    memcpy(vertexBuffer->map(), verts, sizeof(verts));
    vertexBuffer->unmap();
    memcpy(uvBuffer->map(), uvs, sizeof(uvs));
    uvBuffer->unmap();
    memcpy(indexBuffer->map(), indices, sizeof(indices));
    indexBuffer->unmap();
    rendererA->drawImageMesh(image.get(),
                             ImageSampler::LinearClamp(),
                             vertexBuffer,
                             uvBuffer,
                             indexBuffer,
                             4,
                             6,
                             BlendMode::srcOver,
                             1.0f);
    rendererA->drawImageMesh(image.get(),
                             ImageSampler::LinearClamp(),
                             vertexBuffer,
                             uvBuffer,
                             indexBuffer,
                             4,
                             6,
                             BlendMode::srcOver,
                             1.0f,
                             0.625f);
    rendererA->restore();

    SerializingFactory b;
    auto rendererB = b.makeRenderer();
    SerializedReplayHooks hooks;
    hooks.onFrame = [&]() { b.addFrame(); };
    hooks.onFrameSize = [&](uint32_t w, uint32_t h) { b.frameSize(w, h); };
    REQUIRE(replaySerializedCommands(a.bytes(), &b, rendererB.get(), hooks));

    auto sa = a.bytes();
    auto sb = b.bytes();
    REQUIRE(sa.size() == sb.size());
    CHECK(std::memcmp(sa.data(), sb.data(), sa.size()) == 0);
}

namespace
{
// Records one instanced mesh draw, with every per instance field set to
// something distinct so a swapped pair shows up.
struct InstancedMesh
{
    rcp<RenderImage> image;
    rcp<RenderBuffer> pts, uvs, indices;
    rcp<ImageMeshInstances> instances;
    static constexpr size_t Count = 3;
};

rcp<RenderBuffer> recordBuffer(SerializingFactory& f,
                               RenderBufferType type,
                               const void* source,
                               size_t size)
{
    auto buffer = f.makeRenderBuffer(type, RenderBufferFlags::none, size);
    memcpy(buffer->map(), source, size);
    buffer->unmap();
    return buffer;
}

InstancedMesh recordInstancedMesh(SerializingFactory& f, Renderer* renderer)
{
    InstancedMesh mesh;
    auto imageBytes = ReadFile("assets/open_source.jpg");
    mesh.image = f.decodeImage(imageBytes);
    REQUIRE(mesh.image != nullptr);

    const Vec2D quad[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    const Vec2D uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
    mesh.pts = recordBuffer(f, RenderBufferType::vertex, quad, sizeof(quad));
    mesh.uvs = recordBuffer(f, RenderBufferType::vertex, uv, sizeof(uv));
    mesh.indices = recordBuffer(f, RenderBufferType::index, idx, sizeof(idx));

    mesh.instances = f.makeImageMeshInstances(InstancedMesh::Count);
    Span<ImageMeshInstanceData> data = mesh.instances->edit();
    data[0].transform = Mat2D(1, 0, 0, 1, 10, 20);
    data[1].opacity = 0.5f;
    data[2].uvTranslate = {0.25f, 0.5f};
    data[2].uvScale = {0.5f, 0.25f};
    data[2].additiveness = 1.0f;
    mesh.instances->endEdit();

    renderer->drawImageMeshInstanced(mesh.image.get(),
                                     ImageSampler::LinearClamp(),
                                     mesh.pts,
                                     mesh.uvs,
                                     mesh.indices,
                                     4,
                                     6,
                                     mesh.instances);
    return mesh;
}

// Keeps what the instanced draw delivered, and counts the plain mesh draws a
// fallback would have produced instead.
class MeshRecordingRenderer : public NoOpRenderer
{
public:
    int instancedDraws = 0;
    int plainDraws = 0;
    std::vector<ImageMeshInstanceData> got;

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
        got.assign(instances->instanceData().begin(),
                   instances->instanceData().end());
    }
};
} // namespace

TEST_CASE("an instanced mesh draw replays byte-identically",
          "[serialize][replay]")
{
    SerializingFactory a;
    a.frameSize(256, 256);
    a.addFrame();
    auto rendererA = a.makeRenderer();
    recordInstancedMesh(a, rendererA.get());

    SerializingFactory b;
    auto rendererB = b.makeRenderer();
    SerializedReplayHooks hooks;
    hooks.onFrame = [&]() { b.addFrame(); };
    hooks.onFrameSize = [&](uint32_t w, uint32_t h) { b.frameSize(w, h); };
    REQUIRE(replaySerializedCommands(a.bytes(), &b, rendererB.get(), hooks));

    auto sa = a.bytes();
    auto sb = b.bytes();
    REQUIRE(sa.size() == sb.size());
    CHECK(std::memcmp(sa.data(), sb.data(), sa.size()) == 0);
}

// Byte identity only proves the stream survives a rewrite; the writer and
// reader would agree on a swapped pair of fields. Check the values land where
// they were put, and that the draw stays one call rather than fanning out.
TEST_CASE("replayed instances keep their data", "[serialize][replay]")
{
    SerializingFactory a;
    a.frameSize(256, 256);
    a.addFrame();
    auto rendererA = a.makeRenderer();
    recordInstancedMesh(a, rendererA.get());

    SerializingFactory b;
    MeshRecordingRenderer sink;
    REQUIRE(replaySerializedCommands(a.bytes(), &b, &sink));

    CHECK(sink.instancedDraws == 1);
    CHECK(sink.plainDraws == 0);
    REQUIRE(sink.got.size() == InstancedMesh::Count);

    CHECK(sink.got[0].transform[4] == 10.0f);
    CHECK(sink.got[0].transform[5] == 20.0f);
    CHECK(sink.got[1].opacity == 0.5f);
    CHECK(sink.got[2].uvTranslate.x == 0.25f);
    CHECK(sink.got[2].uvTranslate.y == 0.5f);
    CHECK(sink.got[2].uvScale.x == 0.5f);
    CHECK(sink.got[2].uvScale.y == 0.25f);
    CHECK(sink.got[2].additiveness == 1.0f);
    // Untouched entries arrive at the defaults, not zeroed.
    CHECK(sink.got[0].opacity == 1.0f);
    CHECK(sink.got[0].uvScale.x == 1.0f);
    CHECK(sink.got[0].uvScale.y == 1.0f);
}

TEST_CASE("serialized replay rejects a bad header", "[serialize][replay]")
{
    const uint8_t garbage[8] = {'X', 'X', 'X', 'X', 1, 0, 0, 0};
    SerializingFactory b;
    auto r = b.makeRenderer();
    CHECK_FALSE(
        replaySerializedCommands(Span<const uint8_t>(garbage, sizeof(garbage)),
                                 &b,
                                 r.get()));
}

// ---- cache-as-bitmap: offscreen canvas content recorded inline ----
//
// The stream declares a canvas, records its content between
// canvasContentBegin/End, and composites it with an ordinary drawImage of the
// canvas id. Constructing a RenderCanvas allocates nothing, so these stay GPU
// free -- no render context is needed to exercise the protocol.

namespace
{
// Records one stream: an offscreen canvas drawn into, then composited, with a
// plain vector draw on either side of it.
void recordCachedStream(SerializingFactory& f, Renderer* screen)
{
    auto paint = f.makeRenderPaint();
    paint->color(0xFF445566);

    RawPath rp;
    rp.move({0, 0});
    rp.line({10, 0});
    rp.line({10, 10});
    rp.close();
    auto path = f.makeRenderPath(rp, FillRule::nonZero);

    // Before the canvas, so a dropped bracket is distinguishable from a
    // dropped frame.
    screen->drawPath(path.get(), paint.get());

    auto canvas = make_rcp<gpu::RenderCanvas>(64, 48);
    Renderer* content = f.beginCanvasContent(canvas.get(), 0xFF204060);
    REQUIRE(content != nullptr);
    content->save();
    content->transform(Mat2D::fromScale(2.0f, 2.0f));
    content->drawPath(path.get(), paint.get());
    content->restore();
    f.endCanvasContent(canvas.get());

    // The composite: an ordinary drawImage of the canvas id.
    screen->save();
    screen->drawImage(f.contentCanvasImage(canvas.get()).get(),
                      ImageSampler::LinearClamp(),
                      BlendMode::srcOver,
                      1.0f);
    screen->restore();
}

// Counts what actually reaches the screen renderer during a replay.
class CountingRenderer : public NoOpRenderer
{
public:
    void drawPath(RenderPath*, RenderPaint*) override { paths++; }
    void drawImage(const RenderImage*, ImageSampler, BlendMode, float) override
    {
        images++;
    }
    int paths = 0;
    int images = 0;
};
} // namespace

TEST_CASE("canvas content replays byte-identically", "[serialize][replay]")
{
    SerializingFactory a;
    a.frameSize(256, 256);
    a.addFrame();
    auto rendererA = a.makeRenderer();
    recordCachedStream(a, rendererA.get());

    SerializingFactory b;
    auto rendererB = b.makeRenderer();
    // A host with an offscreen target: it mints its own canvas for the id the
    // stream named and hands back the renderer its content records into.
    std::vector<rcp<gpu::RenderCanvas>> canvasesB;
    int begins = 0, ends = 0;
    SerializedReplayHooks hooks;
    hooks.onFrame = [&]() { b.addFrame(); };
    hooks.onFrameSize = [&](uint32_t w, uint32_t h) { b.frameSize(w, h); };
    hooks.onCanvasContentBegin = [&](uint64_t,
                                     uint32_t w,
                                     uint32_t h,
                                     uint32_t clearColor,
                                     rcp<RenderImage>* image) -> Renderer* {
        begins++;
        canvasesB.push_back(make_rcp<gpu::RenderCanvas>(w, h));
        gpu::RenderCanvas* canvas = canvasesB.back().get();
        Renderer* content = b.beginCanvasContent(canvas, clearColor);
        *image = b.contentCanvasImage(canvas);
        return content;
    };
    hooks.onCanvasContentEnd = [&](uint64_t) {
        ends++;
        b.endCanvasContent(canvasesB.back().get());
    };

    REQUIRE(replaySerializedCommands(a.bytes(), &b, rendererB.get(), hooks));
    CHECK(begins == 1);
    CHECK(ends == 1);
    REQUIRE(canvasesB.size() == 1);
    CHECK(canvasesB.front()->width() == 64);
    CHECK(canvasesB.front()->height() == 48);

    auto sa = a.bytes();
    auto sb = b.bytes();
    REQUIRE(sa.size() == sb.size());
    CHECK(std::memcmp(sa.data(), sb.data(), sa.size()) == 0);
}

TEST_CASE("a host with no offscreen target drops the canvas and keeps going",
          "[serialize][replay]")
{
    SerializingFactory a;
    a.frameSize(256, 256);
    a.addFrame();
    auto rendererA = a.makeRenderer();
    recordCachedStream(a, rendererA.get());

    // No onCanvasContentBegin at all: the documented "this host cannot open an
    // offscreen frame" case.
    SerializingFactory b;
    CountingRenderer screen;
    REQUIRE(replaySerializedCommands(a.bytes(), &b, &screen));
    // The plain draw outside the bracket still replays; the canvas's own draw
    // is dropped rather than leaking onto the screen, and the composite finds
    // no image to sample.
    CHECK(screen.paths == 1);
    CHECK(screen.images == 0);
}

TEST_CASE("a canvas end naming the wrong canvas fails replay",
          "[serialize][replay]")
{
    // Bracket depth used to be the only thing checked, so an end whose id did
    // not match its begin still closed the innermost canvas, announced an
    // unrelated id to the host, and reported success.
    SerializingFactory a;
    a.frameSize(256, 256);
    a.addFrame();
    auto rendererA = a.makeRenderer();
    auto paint = a.makeRenderPaint();
    RawPath rp;
    rp.move({0, 0});
    rp.line({10, 10});
    auto path = a.makeRenderPath(rp, FillRule::nonZero);

    // A complete, well-formed bracket first. Its only job is to give `other` a
    // declared id before it is misused below: beginCanvasContent mints ids,
    // and minting one from inside endCanvasContent would interleave a
    // makeRenderCanvas op into the middle of the end op and fail replay for an
    // unrelated reason.
    auto other = make_rcp<gpu::RenderCanvas>(4, 4);
    Renderer* otherContent = a.beginCanvasContent(other.get(), 0);
    otherContent->drawPath(path.get(), paint.get());
    a.endCanvasContent(other.get());

    auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
    Renderer* content = a.beginCanvasContent(canvas.get(), 0);
    content->drawPath(path.get(), paint.get());
    // Closes the open bracket with the other canvas's id.
    a.endCanvasContent(other.get());

    SerializingFactory b;
    CountingRenderer screen;
    int endCount = 0;
    SerializedReplayHooks hooks;
    hooks.onCanvasContentBegin =
        [&](uint64_t, uint32_t, uint32_t, uint32_t, rcp<RenderImage>*)
        -> Renderer* { return &screen; };
    hooks.onCanvasContentEnd = [&](uint64_t) { endCount++; };
    CHECK_FALSE(replaySerializedCommands(a.bytes(), &b, &screen, hooks));
    // The well-formed bracket closed; the mismatched end was rejected before
    // it could tell the host to close a canvas that was never open.
    CHECK(endCount == 1);
}

TEST_CASE("a stream cut inside canvas content fails replay",
          "[serialize][replay]")
{
    SerializingFactory a;
    a.frameSize(256, 256);
    a.addFrame();
    auto rendererA = a.makeRenderer();
    auto paint = a.makeRenderPaint();
    RawPath rp;
    rp.move({0, 0});
    rp.line({10, 10});
    auto path = a.makeRenderPath(rp, FillRule::nonZero);

    auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
    Renderer* content = a.beginCanvasContent(canvas.get(), 0);
    content->drawPath(path.get(), paint.get());
    // Deliberately no endCanvasContent.

    SerializingFactory b;
    CountingRenderer screen;
    SerializedReplayHooks hooks;
    hooks.onCanvasContentBegin =
        [&](uint64_t, uint32_t, uint32_t, uint32_t, rcp<RenderImage>*)
        -> Renderer* { return &screen; };
    CHECK_FALSE(replaySerializedCommands(a.bytes(), &b, &screen, hooks));
}
