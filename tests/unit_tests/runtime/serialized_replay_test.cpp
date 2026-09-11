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
