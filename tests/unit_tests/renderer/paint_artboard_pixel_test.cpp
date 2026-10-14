/*
 * Copyright 2026 Rive
 */

// Pixel-level validation of an artboard used as a paint's image.
//
// The [paint-artboard] cases in runtime/ assert structure against a null
// device: one raster, its size, its cadence. None of that shows whether the
// shape actually ends up filled with the artboard -- the first replayed frame
// used to bind no texture at all, which every structural check passed. So this
// records a host whose shape is painted with a two-color source artboard,
// replays it against a real GPU context, and reads the colors back.

#include "common/testing_window.hpp"

#ifdef RIVE_CANVAS

#include "common/testing_window_deferred_sink.hpp"
#include "rive/artboard.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/paint_image.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive_file_reader.hpp"

#include <catch.hpp>
#include <cmath>
#include <memory>
#include <vector>

using namespace rive;

namespace
{
// A filled rectangle centred at (x, y).
Shape* addRect(Artboard& artboard,
               float x,
               float y,
               float width,
               float height,
               ColorInt color,
               Fill** fillOut = nullptr)
{
    auto* shape = new Shape();
    artboard.addObject(shape);
    shape->parentId(artboard.idOf(&artboard));
    shape->x(x);
    shape->y(y);

    auto* rect = new rive::Rectangle();
    rect->width(width);
    rect->height(height);
    artboard.addObject(rect);
    rect->parentId(artboard.idOf(shape));

    auto* fill = new Fill();
    artboard.addObject(fill);
    fill->parentId(artboard.idOf(shape));

    auto* solid = new SolidColor();
    solid->colorValue(color);
    artboard.addObject(solid);
    solid->parentId(artboard.idOf(fill));

    if (fillOut != nullptr)
    {
        *fillOut = fill;
    }
    return shape;
}

constexpr ColorInt kRed = 0xFFFF0000;
constexpr ColorInt kBlue = 0xFF0000FF;

// RGBA at (x, y) of a w-wide readback.
struct Pixel
{
    uint8_t r, g, b, a;
};
Pixel pixelAt(const std::vector<uint8_t>& px, int w, int x, int y)
{
    size_t i = (static_cast<size_t>(y) * w + x) * 4;
    return {px[i], px[i + 1], px[i + 2], px[i + 3]};
}
bool isRed(Pixel p) { return p.r > 200 && p.g < 40 && p.b < 40; }
bool isBlue(Pixel p) { return p.b > 200 && p.r < 40 && p.g < 40; }

std::unique_ptr<TestingWindow> makeWindow()
{
#if defined(__APPLE__)
    return std::unique_ptr<TestingWindow>(TestingWindow::MakeMetalTexture({}));
#else
    return nullptr;
#endif
}

// A 100x100 source, red on the left half and blue on the right, painted over
// a 200x200 host rectangle.
struct PaintedScene
{
    cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    std::unique_ptr<Artboard> source;
    Shape* sourceRed = nullptr;
    std::unique_ptr<Artboard> host;
    std::unique_ptr<ArtboardInstance> instance;

    explicit PaintedScene(gpu::RenderContext* rc)
    {
        session.bindRenderContext(rc);

        source = std::make_unique<Artboard>(&session);
        source->addObject(source.get());
        sourceRed = addRect(*source, 25, 50, 50, 100, kRed);
        addRect(*source, 75, 50, 50, 100, kBlue);
        REQUIRE(source->initialize() == StatusCode::Ok);
        source->width(100);
        source->height(100);

        host = std::make_unique<Artboard>(&session);
        host->addObject(host.get());
        Fill* fill = nullptr;
        addRect(*host, 100, 100, 200, 200, 0xFFFFFFFF, &fill);
        auto* paint = new PaintImage();
        host->addObject(paint);
        paint->parentId(host->idOf(fill));
        REQUIRE(host->initialize() == StatusCode::Ok);
        host->width(200);
        host->height(200);
        paint->referencedArtboard(source.get());

        instance = host->instance<ArtboardInstance>(&session);
        REQUIRE(instance != nullptr);
        instance->advance(0.0f);
    }

    // The instance's own copy of the source's red rectangle.
    Shape* innerRed()
    {
        for (Core* object : instance->objects())
        {
            if (object != nullptr && object->is<PaintImage>())
            {
                auto* source = object->as<PaintImage>()->artboardSource();
                REQUIRE(source != nullptr);
                for (Core* inner : source->artboard()->objects())
                {
                    if (inner != nullptr && inner->is<Shape>())
                    {
                        // The first shape added is the red one.
                        return inner->as<Shape>();
                    }
                }
            }
        }
        return nullptr;
    }

    std::vector<uint8_t> render(TestingWindow* window,
                                gpu::RenderContext* rc,
                                cmd::DeferredReplayer& replayer)
    {
        window->resize(200, 200);
        window->beginFrame(
            {.name = "paint_artboard", .clearColor = 0xff000000});
        auto mainDesc = rc->frameDescriptor();
        Renderer* screen = session.screenRenderer();
        instance->drawInternal(screen);
        cmd::DeferredFrame frame = cmd::snapshotFrame(session);
        session.resetFrame();
        rive_tests::TestingWindowFrameSink sink(window, rc, mainDesc);
        replayer.replayFrame(frame, sink);
        std::vector<uint8_t> pixels;
        window->endFrame(&pixels);
        return pixels;
    }
};
} // namespace

TEST_CASE("a shape painted with an artboard shows it on the first frame",
          "[paint-artboard]")
{
    auto window = makeWindow();
    if (window == nullptr || window->renderContext() == nullptr)
    {
        WARN("no GPU render context available, skipping");
        return;
    }
    auto* rc = window->renderContext();
    PaintedScene scene(rc);
    cmd::DeferredReplayer replayer;

    auto pixels = scene.render(window.get(), rc, replayer);
    // The source stretched over the shape: red on the left, blue on the
    // right. Without a texture the white paint would show through instead.
    CHECK(isRed(pixelAt(pixels, 200, 50, 100)));
    CHECK(isBlue(pixelAt(pixels, 200, 150, 100)));
}

TEST_CASE("a source bound to the host's view model shows its value",
          "[paint-artboard]")
{
    // assets/paint_artboard_bind.riv (rive_core's
    // generate_paint_artboard_bind_riv_test.dart): the same red/blue source as
    // above, with the red half's x bound to the host's "redX".
    auto window = makeWindow();
    if (window == nullptr || window->renderContext() == nullptr)
    {
        WARN("no GPU render context available, skipping");
        return;
    }
    auto* rc = window->renderContext();
    cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(rc);
    auto file = ReadRiveFile("assets/paint_artboard_bind.riv", &session);
    auto host = file->artboardNamed("Host");
    REQUIRE(host != nullptr);
    auto instance = file->viewModelByName("vm")->createDefaultInstance();
    REQUIRE(instance != nullptr);
    host->bindViewModelInstance(instance->instance());
    cmd::DeferredReplayer replayer;

    auto render = [&]() {
        host->advance(0.016f);
        window->resize(200, 200);
        window->beginFrame(
            {.name = "paint_artboard_bind", .clearColor = 0xff000000});
        auto mainDesc = rc->frameDescriptor();
        host->drawInternal(session.screenRenderer());
        cmd::DeferredFrame frame = cmd::snapshotFrame(session);
        session.resetFrame();
        rive_tests::TestingWindowFrameSink sink(window.get(), rc, mainDesc);
        replayer.replayFrame(frame, sink);
        std::vector<uint8_t> pixels;
        window->endFrame(&pixels);
        return pixels;
    };

    // The default instance nudges the red half 5 units right of where it was
    // authored (30 rather than 25); both halves still read the same way.
    auto first = render();
    CHECK(isRed(pixelAt(first, 200, 50, 100)));
    CHECK(isBlue(pixelAt(first, 200, 150, 100)));

    // Moving it through the view model moves it on screen.
    instance->propertyNumber("redX")->value(75.0f);
    auto second = render();
    CHECK(isRed(pixelAt(second, 200, 150, 100)));
}

TEST_CASE("a changed source shows on the next frame", "[paint-artboard]")
{
    auto window = makeWindow();
    if (window == nullptr || window->renderContext() == nullptr)
    {
        WARN("no GPU render context available, skipping");
        return;
    }
    auto* rc = window->renderContext();
    PaintedScene scene(rc);
    cmd::DeferredReplayer replayer;
    auto first = scene.render(window.get(), rc, replayer);
    REQUIRE(isBlue(pixelAt(first, 200, 150, 100)));

    // Slide the red half over the blue one inside the paint's own instance.
    Shape* red = scene.innerRed();
    REQUIRE(red != nullptr);
    red->x(75);
    scene.instance->advance(0.016f);

    auto second = scene.render(window.get(), rc, replayer);
    CHECK(isRed(pixelAt(second, 200, 150, 100)));

    // A frame with no change composites the raster it already has.
    scene.instance->advance(0.016f);
    auto third = scene.render(window.get(), rc, replayer);
    CHECK(isRed(pixelAt(third, 200, 150, 100)));
}

#endif
