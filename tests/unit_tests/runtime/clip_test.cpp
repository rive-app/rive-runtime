#include <rive/clip_result.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include <catch.hpp>

using namespace rive;

TEST_CASE("clipping loads correctly", "[clipping]")
{
    auto file = ReadRiveFile("assets/circle_clips.riv");

    auto node = file->artboard()->find("TopEllipse");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Shape>());

    auto shape = node->as<rive::Shape>();
    REQUIRE(shape->clippingShapes().size() == 2);
    REQUIRE(shape->clippingShapes()[0]->source()->name() == "ClipRect2");
    REQUIRE(shape->clippingShapes()[1]->source()->name() == "BabyEllipse");

    file->artboard()->updateComponents();

    rive::NoOpRenderer renderer;
    file->artboard()->draw(&renderer);
}

class ClipTestRenderPath : public rive::RenderPath
{
public:
    rive::RawPath rawPath;
    ClipTestRenderPath(rive::RawPath& path) : rawPath(path) {}

    void rewind() override {}

    void fillRule(rive::FillRule value) override {}
    void addPath(rive::CommandPath* path, const rive::Mat2D& transform) override
    {}
    void addRenderPath(rive::RenderPath* path,
                       const rive::Mat2D& transform) override
    {}
    void addRawPath(const rive::RawPath& path) override
    {
        rawPath.addPath(path, nullptr);
    }

    void moveTo(float x, float y) override {}
    void lineTo(float x, float y) override {}
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {}
    void close() override {}
};

class ClippingFactory : public rive::NoOpFactory
{
    rive::rcp<rive::RenderPath> makeRenderPath(rive::RawPath& rawPath,
                                               rive::FillRule) override
    {
        return rive::make_rcp<ClipTestRenderPath>(rawPath);
    }
};

TEST_CASE("artboard is clipped correctly", "[clipping]")
{
    ClippingFactory factory;
    auto file = ReadRiveFile("assets/artboardclipping.riv", &factory);

    auto artboard = file->artboard("Center");
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);
    REQUIRE(artboard->originX() == 0.5);
    REQUIRE(artboard->originY() == 0.5);
    {
        auto clipPath = artboard->worldPath()->rawPath();
        auto points = clipPath->points();
        REQUIRE(points.size() == 4);

        REQUIRE(points[0] == rive::Vec2D(0.0f, 0.0f));
        REQUIRE(points[1] == rive::Vec2D(500.0f, 0.0f));
        REQUIRE(points[2] == rive::Vec2D(500.0f, 500.0f));
        REQUIRE(points[3] == rive::Vec2D(0.0f, 500.0f));
    }
    // Now disable framing the origin so that the points are in origin space.
    artboard->frameOrigin(false);
    artboard->updateComponents();
    {
        // auto clipPath =
        // static_cast<ClipTestRenderPath*>(artboard->clipPath());
        auto clipPath = artboard->worldPath()->rawPath();
        auto points = clipPath->points();
        REQUIRE(points.size() == 4);

        REQUIRE(points[0] == rive::Vec2D(-250.0f, -250.0f));
        REQUIRE(points[1] == rive::Vec2D(250.0f, -250.0f));
        REQUIRE(points[2] == rive::Vec2D(250.0f, 250.0f));
        REQUIRE(points[3] == rive::Vec2D(-250.0f, 250.0f));
    }
}

TEST_CASE("Apply clipping to elements that are moved outside their hierarchy",
          "[clipping]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/clipping_and_draw_order.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.16f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.16f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("clipping_and_draw_order"));
}
