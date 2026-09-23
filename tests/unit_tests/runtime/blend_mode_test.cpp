/*
 * Copyright 2026 Rive
 */

// BlendMode::additive is the only parameterized blend mode: how much of it
// mixes in is carried by a separate uint8 property, additiveAmount, on both
// Drawable and ShapePaint.

#include <rive/artboard.hpp>
#include <rive/shapes/paint/blend_mode.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/shape.hpp>

#include "rive_render_paint.hpp"
#ifdef WITH_RIVE_SCRIPTING_LUAU
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "utils/no_op_factory.hpp"

#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include <catch.hpp>

using namespace rive;

TEST_CASE("additive is a valid blend mode", "[blend]")
{
    // 12 is Flutter's ui.BlendMode.plus index, which the canvas-backed
    // renderers decode straight from the paint stream.
    REQUIRE(static_cast<int>(BlendMode::additive) == 12);
    REQUIRE(static_cast<uint32_t>(BlendMode::additive) <
            (1u << BLEND_MODE_BIT_COUNT));
}

TEST_CASE("ShapePaint carries its own additive amount", "[blend]")
{
    Fill fill;

    // Fully additive out of the box, so picking the mode looks additive
    // without also having to set the amount.
    REQUIRE(fill.additiveAmount() == 255);
    REQUIRE(fill.blendModeValue() == 127); // 127 means inherit

    fill.blendModeValue((uint32_t)BlendMode::additive);
    fill.additiveAmount(128);
    REQUIRE(fill.additiveAmount() == 128);
    REQUIRE(fill.blendModeValue() == 12);

    // The two are independent: changing the mode leaves the amount alone.
    fill.blendModeValue((uint32_t)BlendMode::multiply);
    REQUIRE(fill.additiveAmount() == 128);
}

TEST_CASE("Drawable validates blend mode values on load", "[blend]")
{
    // onAddedDirty re-runs a component's registration, so each case gets its
    // own freshly imported artboard rather than re-validating one in place.
    auto check = [](uint32_t blendModeValue) {
        auto file = ReadRiveFile("assets/shapetest.riv");
        auto artboard = file->artboard()->instance();
        auto shape = artboard->find<Shape>().front();
        REQUIRE(shape != nullptr);
        shape->blendModeValue(blendModeValue);
        return shape->onAddedDirty(artboard.get());
    };

    // Every legacy mode still loads, and so does additive.
    REQUIRE(check(3) == StatusCode::Ok);
    REQUIRE(check(28) == StatusCode::Ok);
    REQUIRE(check((uint32_t)BlendMode::additive) == StatusCode::Ok);

    // Unknown modes are still rejected.
    REQUIRE(check(13) == StatusCode::InvalidObject);
    REQUIRE(check(0) == StatusCode::InvalidObject);
}

TEST_CASE("a loaded drawable reports its blend mode and amount", "[blend]")
{
    auto file = ReadRiveFile("assets/shapetest.riv");
    auto artboard = file->artboard()->instance();
    auto shape = artboard->find<Shape>().front();
    REQUIRE(shape != nullptr);

    shape->blendModeValue((uint32_t)BlendMode::additive);
    shape->additiveAmount(200);
    REQUIRE(shape->blendMode() == BlendMode::additive);
    REQUIRE(shape->additiveAmount() == 200);

    shape->blendModeValue(28);
    REQUIRE(shape->blendMode() == BlendMode::luminosity);
    REQUIRE(shape->additiveAmount() == 200);
}

TEST_CASE("additive resolves to srcOver plus additiveness on the GPU paint",
          "[blend]")
{
    // The GPU pipeline has no additive blend id and ignores additiveness on
    // anything but srcOver, so RiveRenderPaint folds additive into srcOver and
    // carries the mix in additiveness instead.
    RiveRenderPaint paint;

    paint.blendMode(BlendMode::additive);
    REQUIRE(paint.getBlendMode() == BlendMode::srcOver);

    paint.additiveness(128 / 255.0f);
    REQUIRE(paint.getAdditiveness() == Approx(128 / 255.0f));
    // An additive draw is never opaque, so it can't take the opaque fast path.
    paint.color(0xFFFFFFFF);
    REQUIRE(!paint.getIsOpaque());

    // Every other mode passes through untouched.
    paint.blendMode(BlendMode::multiply);
    REQUIRE(paint.getBlendMode() == BlendMode::multiply);

    // ...and with no additiveness an opaque solid stays on the fast path.
    paint.blendMode(BlendMode::srcOver);
    paint.additiveness(0);
    REQUIRE(paint.getIsOpaque());
}

namespace
{
// ReadRiveFile builds paints from a NoOpFactory, which discards everything.
// This records just what ShapePaint::blendMode() is supposed to push.
class RecordingPaint : public rive::RenderPaint
{
public:
    rive::BlendMode blend = rive::BlendMode::srcOver;
    float additive = -1.f;

    void style(rive::RenderPaintStyle) override {}
    void color(rive::ColorInt) override {}
    void thickness(float) override {}
    void join(rive::StrokeJoin) override {}
    void cap(rive::StrokeCap) override {}
    void blendMode(rive::BlendMode value) override { blend = value; }
    void additiveness(float value) override { additive = value; }
    void shader(rive::rcp<rive::RenderShader>) override {}
    void invalidateStroke() override {}
};

class RecordingFactory : public rive::NoOpFactory
{
public:
    rive::rcp<rive::RenderPaint> makeRenderPaint() override
    {
        return rive::make_rcp<RecordingPaint>();
    }
};
} // namespace

TEST_CASE("a ShapePaint syncs mode and amount onto its RenderPaint", "[blend]")
{
    RecordingFactory factory;
    auto file = ReadRiveFile("assets/shapetest.riv", &factory);
    auto artboard = file->artboard()->instance();
    auto fill = artboard->find<Fill>().front();
    REQUIRE(fill != nullptr);
    auto* rp = static_cast<RecordingPaint*>(fill->renderPaint());
    REQUIRE(rp != nullptr);

    // Its own mode wins over the parent's, and carries its own amount.
    fill->blendModeValue((uint32_t)BlendMode::additive);
    fill->additiveAmount(255);
    fill->blendMode(BlendMode::multiply, 0);
    REQUIRE(rp->blend == BlendMode::additive);
    REQUIRE(rp->additive == Approx(1.0f));

    // 127 inherits both the mode and the amount from the parent drawable.
    fill->blendModeValue(127);
    fill->additiveAmount(0);
    fill->blendMode(BlendMode::additive, 64);
    REQUIRE(rp->blend == BlendMode::additive);
    REQUIRE(rp->additive == Approx(64 / 255.0f));

    // A non-additive mode leaves no additiveness behind.
    fill->blendMode(BlendMode::screen, 200);
    REQUIRE(rp->blend == BlendMode::screen);
    REQUIRE(rp->additive == 0.0f);
}

TEST_CASE("changing the amount re-syncs the paint after load", "[blend]")
{
    // additiveAmount animates and data binds, so it changes long after
    // buildDependencies() ran its one-shot sync. Without the changed hooks the
    // byte moves but the RenderPaint keeps its load-time additiveness, and an
    // animated or bound amount renders nothing.
    RecordingFactory factory;
    auto file = ReadRiveFile("assets/shapetest.riv", &factory);
    auto artboard = file->artboard()->instance();
    // Pair a shape with a fill it actually owns; find<>() order says nothing
    // about the parent/child relationship the sync depends on.
    Shape* shape = nullptr;
    Fill* fill = nullptr;
    for (auto candidate : artboard->find<Fill>())
    {
        if (candidate->parent() != nullptr && candidate->parent()->is<Shape>())
        {
            fill = candidate;
            shape = candidate->parent()->as<Shape>();
            break;
        }
    }
    REQUIRE(fill != nullptr);
    REQUIRE(shape != nullptr);
    auto* rp = static_cast<RecordingPaint*>(fill->renderPaint());

    // A paint left on 127 inherits, so the drawable drives it. Note the
    // property defaults to 255: it has to move off that for the hook to fire.
    fill->blendModeValue(127);
    shape->blendModeValue((uint32_t)BlendMode::additive);
    shape->additiveAmount(64);
    REQUIRE(rp->blend == BlendMode::additive);
    REQUIRE(rp->additive == Approx(64 / 255.0f));

    shape->additiveAmount(200);
    REQUIRE(rp->additive == Approx(200 / 255.0f));

    // A paint with its own mode drives its own amount instead.
    fill->blendModeValue((uint32_t)BlendMode::additive);
    fill->additiveAmount(128);
    REQUIRE(rp->blend == BlendMode::additive);
    REQUIRE(rp->additive == Approx(128 / 255.0f));

    // Back to inheriting: the paint's own amount is ignored, so writing it
    // must not stomp what the drawable pushed down.
    fill->blendModeValue(127);
    shape->additiveAmount(32);
    REQUIRE(rp->additive == Approx(32 / 255.0f));
    fill->additiveAmount(5);
    REQUIRE(rp->additive == Approx(32 / 255.0f));
}

#ifdef WITH_RIVE_SCRIPTING_LUAU
TEST_CASE("a scripted paint sets additiveness alongside the mode", "[blend]")
{
    // A script names a blend mode but never an amount, and the GPU paint folds
    // additive into srcOver -- so the mode alone would render plain srcOver.
    // ScriptedPaint pushes full additiveness with it, and clears it again when
    // the script switches away.
    RecordingFactory factory;
    ScriptedPaint paint(&factory);
    auto* rp = static_cast<RecordingPaint*>(paint.renderPaint.get());
    REQUIRE(rp != nullptr);

    paint.blendMode(BlendMode::additive);
    CHECK(rp->blend == BlendMode::additive);
    CHECK(rp->additive == Approx(1.0f));

    paint.blendMode(BlendMode::multiply);
    CHECK(rp->blend == BlendMode::multiply);
    CHECK(rp->additive == Approx(0.0f));

    // The copy constructor replays the source's state through the same
    // setters, so a cloned additive paint carries the amount too.
    paint.blendMode(BlendMode::additive);
    ScriptedPaint clone(&factory, paint);
    auto* cloneRp = static_cast<RecordingPaint*>(clone.renderPaint.get());
    REQUIRE(cloneRp != nullptr);
    CHECK(cloneRp->blend == BlendMode::additive);
    CHECK(cloneRp->additive == Approx(1.0f));
}
#endif

TEST_CASE("an animated additive amount records its blend mode and amount "
          "every frame",
          "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/additive_blendmode_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("additive_blendmode_test"));
}
