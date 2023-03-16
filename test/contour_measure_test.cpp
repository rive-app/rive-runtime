/*
 * Copyright 2022 Rive
 */

#include <rive/math/contour_measure.hpp>
#include <rive/math/math_types.hpp>
#include <rive/math/raw_path.hpp>
#include <rive/math/vec2d.hpp>

#include "rive_file_reader.hpp"
#include "rive/animation/state_machine_instance.hpp"

#include <catch.hpp>
#include <cstdio>

using namespace rive;

static bool nearly_eq(float a, float b, float tolerance)
{
    assert(tolerance >= 0);
    const float diff = std::abs(a - b);
    const float max = std::max(std::abs(a), std::abs(b));
    const float allowed = tolerance * max;
    if (diff > allowed)
    {
        printf("%g %g delta %g allowed %g\n", a, b, diff, allowed);
        return false;
    }
    return true;
}

static bool nearly_eq(Vec2D a, Vec2D b, float tol)
{
    return nearly_eq(a.x, b.x, tol) && nearly_eq(a.y, b.y, tol);
}

TEST_CASE("contour-basics", "[contourmeasure]")
{
    const float tol = 0.000001f;

    RawPath path;
    ContourMeasureIter iter(path, false);
    REQUIRE(iter.next() == nullptr);

    path.moveTo(1, 2);
    iter.rewind(path, false);
    REQUIRE(iter.next() == nullptr);

    path.lineTo(4, 6);
    iter.rewind(path, false);
    auto cm = iter.next();
    REQUIRE(cm);
    REQUIRE(nearly_eq(cm->length(), 5, tol));
    REQUIRE(iter.next() == nullptr);

    // check the mid-points of a rectangle

    path = RawPath();
    const float w = 4, h = 6;
    path.addRect({0, 0, w, h}, PathDirection::cw);
    iter.rewind(path, false);
    cm = iter.next();
    REQUIRE(cm);
    REQUIRE(nearly_eq(cm->length(), 2 * (w + h), tol));
    const float midDistances[] = {
        w / 2,
        w + h / 2,
        w + h + w / 2,
        w + h + w + h / 2,
    };
    const ContourMeasure::PosTan midPoints[] = {
        {{w / 2, 0}, {1, 0}},
        {{w, h / 2}, {0, 1}},
        {{w / 2, h}, {-1, 0}},
        {{0, h / 2}, {0, -1}},
    };
    for (int i = 0; i < 4; ++i)
    {
        auto rec = cm->getPosTan(midDistances[i]);
        REQUIRE(nearly_eq(rec.pos, midPoints[i].pos, tol));
        REQUIRE(nearly_eq(rec.tan, midPoints[i].tan, tol));
    }
    REQUIRE(iter.next() == nullptr);
}

TEST_CASE("multi-contours", "[contourmeasure]")
{
    const Vec2D pts[] = {
        {0, 0},
        {3, 0},
        {3, 4},
    };
    auto span = make_span(pts, sizeof(pts) / sizeof(pts[0]));

    // We expect 3 measurable contours out of this: 7, 16, 7
    // the others should be skipped since they are empty (len == 0)

    RawPath path;
    path.addPoly(span, false); // len == 7

    path.addPoly(span, true); // len == 12

    // should be skipped (lengh == 0)
    path.moveTo(0, 0);

    // should be skipped (lengh == 0)
    path.moveTo(0, 0);
    path.close();

    // should be skipped (lengh == 0)
    path.moveTo(0, 0);
    path.lineTo(0, 0);

    // should be skipped (lengh == 0)
    path.moveTo(0, 0);
    path.lineTo(0, 0);
    path.close();

    path.addPoly(span, false); // len == 7

    ContourMeasureIter iter(path, false);
    auto cm = iter.next();
    REQUIRE(cm->length() == 7);
    cm = iter.next();
    REQUIRE(cm->length() == 12);
    cm = iter.next();
    REQUIRE(cm->length() == 7);
    cm = iter.next();
    REQUIRE(!cm);
}

TEST_CASE("contour-oval", "[contourmeasure]")
{
    const float tol = 0.0075f;

    const float r = 10;
    RawPath path;
    path.addOval({-r, -r, r, r}, PathDirection::cw);
    ContourMeasureIter iter(path, false);

    auto cm = iter.next();
    REQUIRE(nearly_eq(cm->length(), 2 * r * math::PI, tol));
    REQUIRE(!iter.next());
}

TEST_CASE("bad contour", "[contourmeasure]")
{
    auto file = ReadRiveFile("../../test/assets/zombie_skins.riv");

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->advanceAndApply(0.0f);
}
