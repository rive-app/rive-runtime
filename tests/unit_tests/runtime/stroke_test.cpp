#include <rive/animation/keyframe_uint.hpp>
#include <rive/file.hpp>
#include <rive/generated/core_registry.hpp>
#include <rive/node.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/color.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("stroke can be looked up at runtime", "[file]")
{
    auto file = ReadRiveFile("assets/stroke_name_test.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard->find<rive::Stroke>("white_stroke") != nullptr);
    auto stroke = artboard->find<rive::Stroke>("white_stroke");
    REQUIRE(stroke->paint()->is<rive::SolidColor>());
    stroke->paint()->as<rive::SolidColor>()->colorValue(
        rive::colorARGB(255, 0, 255, 255));
}

namespace
{
// ReadRiveFile builds paints from a NoOpFactory, which discards everything.
// This records the stroke position Stroke pushes onto its RenderPaint.
class StrokePositionPaint : public rive::RenderPaint
{
public:
    rive::StrokePosition position = rive::StrokePosition::center;
    int positionWrites = 0;

    void style(rive::RenderPaintStyle) override {}
    void color(rive::ColorInt) override {}
    void thickness(float) override {}
    void join(rive::StrokeJoin) override {}
    void cap(rive::StrokeCap) override {}
    void strokePosition(rive::StrokePosition value) override
    {
        position = value;
        positionWrites++;
    }
    void blendMode(rive::BlendMode) override {}
    void shader(rive::rcp<rive::RenderShader>) override {}
    void invalidateStroke() override {}
};

class StrokePositionFactory : public rive::NoOpFactory
{
public:
    rive::rcp<rive::RenderPaint> makeRenderPaint() override
    {
        return rive::make_rcp<StrokePositionPaint>();
    }
};
} // namespace

TEST_CASE("stroke position defaults to center", "[stroke]")
{
    StrokePositionFactory factory;
    auto file = ReadRiveFile("assets/stroke_name_test.riv", &factory);
    auto artboard = file->artboard()->instance();
    auto stroke = artboard->find<rive::Stroke>("white_stroke");
    REQUIRE(stroke != nullptr);

    // Files written before the property existed never carry it.
    CHECK(stroke->position() == (uint8_t)rive::StrokePosition::center);
    CHECK(stroke->strokePosition() == rive::StrokePosition::center);
    auto* paint = static_cast<StrokePositionPaint*>(stroke->renderPaint());
    REQUIRE(paint != nullptr);
    CHECK(paint->positionWrites > 0);
    CHECK(paint->position == rive::StrokePosition::center);
}

TEST_CASE("changing stroke position reaches the render paint", "[stroke]")
{
    StrokePositionFactory factory;
    auto file = ReadRiveFile("assets/stroke_name_test.riv", &factory);
    auto artboard = file->artboard()->instance();
    auto stroke = artboard->find<rive::Stroke>("white_stroke");
    REQUIRE(stroke != nullptr);
    auto* paint = static_cast<StrokePositionPaint*>(stroke->renderPaint());
    REQUIRE(paint != nullptr);

    stroke->position((uint8_t)rive::StrokePosition::inside);
    artboard->advance(0.0f);
    CHECK(paint->position == rive::StrokePosition::inside);

    stroke->position((uint8_t)rive::StrokePosition::outside);
    artboard->advance(0.0f);
    CHECK(paint->position == rive::StrokePosition::outside);

    // A value from a newer file this runtime doesn't know draws centered.
    stroke->position(7);
    artboard->advance(0.0f);
    CHECK(stroke->strokePosition() == rive::StrokePosition::center);
    CHECK(paint->position == rive::StrokePosition::center);
}

TEST_CASE("stroke applyTo carries its position", "[stroke]")
{
    StrokePositionFactory factory;
    auto file = ReadRiveFile("assets/stroke_name_test.riv", &factory);
    auto artboard = file->artboard()->instance();
    auto stroke = artboard->find<rive::Stroke>("white_stroke");
    REQUIRE(stroke != nullptr);

    // Text and layout paints get their stroke params through applyTo.
    stroke->position((uint8_t)rive::StrokePosition::outside);
    StrokePositionPaint target;
    stroke->applyTo(&target, 1.0f);
    CHECK(target.position == rive::StrokePosition::outside);
}

TEST_CASE("keyframes and binds drive stroke position", "[stroke]")
{
    // KeyFrameUint and the enum/number data bind contexts all land in
    // CoreRegistry::setUint, so both reach the paint through positionChanged.
    StrokePositionFactory factory;
    auto file = ReadRiveFile("assets/stroke_name_test.riv", &factory);
    auto artboard = file->artboard()->instance();
    auto stroke = artboard->find<rive::Stroke>("white_stroke");
    REQUIRE(stroke != nullptr);
    auto* paint = static_cast<StrokePositionPaint*>(stroke->renderPaint());
    REQUIRE(paint != nullptr);

    rive::KeyFrameUint keyframe;
    keyframe.value((uint32_t)rive::StrokePosition::outside);
    keyframe.apply(stroke, rive::StrokeBase::positionPropertyKey, 1.0f);
    artboard->advance(0.0f);
    CHECK(paint->position == rive::StrokePosition::outside);

    rive::CoreRegistry::setUint(stroke,
                                rive::StrokeBase::positionPropertyKey,
                                (uint32_t)rive::StrokePosition::inside);
    artboard->advance(0.0f);
    CHECK(paint->position == rive::StrokePosition::inside);
}

TEST_CASE("stroke position holds between keyframes", "[stroke]")
{
    // An enum has nothing between its values, so a partial mix (a blend
    // state, or a transition) must switch rather than land on a neighbor.
    CHECK_FALSE(rive::CoreRegistry::isInterpolatableUint(
        rive::StrokeBase::positionPropertyKey));

    StrokePositionFactory factory;
    auto file = ReadRiveFile("assets/stroke_name_test.riv", &factory);
    auto artboard = file->artboard()->instance();
    auto stroke = artboard->find<rive::Stroke>("white_stroke");
    REQUIRE(stroke != nullptr);

    rive::KeyFrameUint keyframe;
    keyframe.value((uint32_t)rive::StrokePosition::inside);
    // Center (1) mixed halfway toward inside (0) would round to either
    // neighbor if it interpolated.
    keyframe.apply(stroke, rive::StrokeBase::positionPropertyKey, 0.5f);
    CHECK(stroke->position() == (uint8_t)rive::StrokePosition::inside);
}
