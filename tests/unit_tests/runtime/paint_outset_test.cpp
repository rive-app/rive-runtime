/*
 * Copyright 2026 Rive
 *
 * The one definition of how far past its geometry a paint reaches. Two
 * independent consumers depend on this table agreeing:
 *
 *   - RiveRenderPath::calculateBoundsOutset, which culls draws;
 *   - offscreen raster sizing, which decides how big a layer-mask surface is.
 *
 * and a third, the editor's Dart mirror, is pinned against the same numbers.
 * If a case here changes, the Dart table has to change with it or the editor
 * preview will crop where playback does not.
 */

#include <rive/shapes/paint/paint_outset.hpp>

#include <catch.hpp>

using namespace rive;

TEST_CASE("no stroke and no feather reaches nowhere", "[paint-outset]")
{
    CHECK(paintBoundsOutset({}, 0.0f) == 0.0f);
}

TEST_CASE("a stroke reaches half its thickness", "[paint-outset]")
{
    StrokeParams s;
    s.thickness = 10.0f;
    s.join = StrokeJoin::round;
    s.cap = StrokeCap::butt;
    CHECK(paintBoundsOutset(s, 0.0f) == Approx(5.0f));

    // Zero thickness is a real authored state and must not be special-cased
    // into something non-zero.
    s.thickness = 0.0f;
    CHECK(paintBoundsOutset(s, 0.0f) == 0.0f);
}

TEST_CASE("a miter join overshoots the corner by the miter limit",
          "[paint-outset]")
{
    StrokeParams s;
    s.thickness = 10.0f;
    s.join = StrokeJoin::miter;
    s.cap = StrokeCap::butt;
    CHECK(paintBoundsOutset(s, 0.0f) == Approx(5.0f * kMiterLimit));
    CHECK(paintBoundsOutset(s, 0.0f) == Approx(20.0f));
}

TEST_CASE("a square cap overshoots the end by its diagonal", "[paint-outset]")
{
    StrokeParams s;
    s.thickness = 10.0f;
    s.join = StrokeJoin::round;
    s.cap = StrokeCap::square;
    CHECK(paintBoundsOutset(s, 0.0f) == Approx(5.0f * math::SQRT2));

    // Bevel joins do not overshoot, so the cap still decides.
    s.join = StrokeJoin::bevel;
    CHECK(paintBoundsOutset(s, 0.0f) == Approx(5.0f * math::SQRT2));
}

TEST_CASE("a miter join outranks a square cap", "[paint-outset]")
{
    // Not additive, and the miter branch wins: it is the larger of the two and
    // the two overshoots cannot both happen at the same vertex.
    StrokeParams s;
    s.thickness = 10.0f;
    s.join = StrokeJoin::miter;
    s.cap = StrokeCap::square;
    CHECK(paintBoundsOutset(s, 0.0f) == Approx(5.0f * kMiterLimit));
}

TEST_CASE("a feather reaches 1.5x its authored strength", "[paint-outset]")
{
    // Design tools quote a blur as the width of two standard deviations, so an
    // authored 10 bleeds 15 in every direction.
    CHECK(featherRadiusFromFeather(10.0f) == Approx(15.0f));
    CHECK(paintBoundsOutset({}, 10.0f) == Approx(15.0f));
    CHECK(paintBoundsOutset({}, 0.0f) == 0.0f);
}

TEST_CASE("a feather adds to a stroke rather than replacing it",
          "[paint-outset]")
{
    // These two DO compound: the stroke widens the shape and the blur spreads
    // the widened edge.
    StrokeParams s;
    s.thickness = 10.0f;
    s.join = StrokeJoin::round;
    s.cap = StrokeCap::butt;
    CHECK(paintBoundsOutset(s, 10.0f) == Approx(5.0f + 15.0f));

    s.join = StrokeJoin::miter;
    CHECK(paintBoundsOutset(s, 10.0f) == Approx(20.0f + 15.0f));
}

TEST_CASE("the outset never shrinks the box", "[paint-outset]")
{
    // Whatever combination is authored, the result is an over-estimate or exact
    // -- never negative, which would crop.
    for (float thickness : {0.0f, 0.5f, 1.0f, 40.0f})
    {
        for (float feather : {0.0f, 0.25f, 30.0f})
        {
            for (auto join :
                 {StrokeJoin::miter, StrokeJoin::round, StrokeJoin::bevel})
            {
                for (auto cap :
                     {StrokeCap::butt, StrokeCap::round, StrokeCap::square})
                {
                    StrokeParams s;
                    s.thickness = thickness;
                    s.join = join;
                    s.cap = cap;
                    const float outset = paintBoundsOutset(s, feather);
                    CHECK(outset >= 0.0f);
                    // Must cover the naive half-thickness plus the blur.
                    CHECK(outset >= Approx(thickness * 0.5f +
                                           featherRadiusFromFeather(feather))
                                        .margin(1e-4f));
                }
            }
        }
    }
}
