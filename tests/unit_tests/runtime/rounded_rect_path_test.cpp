/*
 * Copyright 2026 Rive
 */

// Path::addRoundedRect lets LayoutComponent draw its background without
// holding a whole Rectangle (1024 bytes per layout) purely to build a path.
#include <rive/math/aabb.hpp>
#include <rive/math/raw_path.hpp>
#include <rive/shapes/path.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape_paint_path.hpp>

#include <catch.hpp>
#include <vector>

using namespace rive;

namespace
{
// The path a Rectangle builds for these dimensions and radii, via the same
// Path::buildPath every other shape uses.
RawPath viaRectangle(float width,
                     float height,
                     float tl,
                     float tr,
                     float br,
                     float bl)
{
    Rectangle rect;
    rect.originX(0.0f);
    rect.originY(0.0f);
    rect.width(width);
    rect.height(height);
    rect.linkCornerRadius(false);
    rect.cornerRadiusTL(tl);
    rect.cornerRadiusTR(tr);
    rect.cornerRadiusBR(br);
    rect.cornerRadiusBL(bl);
    rect.update(ComponentDirt::Path);
    return rect.rawPath();
}

RawPath viaHelper(float width,
                  float height,
                  float tl,
                  float tr,
                  float br,
                  float bl)
{
    RawPath path;
    Path::addRoundedRect(path, AABB{0.0f, 0.0f, width, height}, tl, tr, br, bl);
    return path;
}

void requireSamePath(const RawPath& expected,
                     const RawPath& actual,
                     const char* label)
{
    std::vector<PathVerb> expectedVerbs, actualVerbs;
    std::vector<Vec2D> expectedPoints, actualPoints;
    for (auto [verb, pts] : expected)
    {
        expectedVerbs.push_back(verb);
    }
    for (auto [verb, pts] : actual)
    {
        actualVerbs.push_back(verb);
    }
    for (auto point : expected.points())
    {
        expectedPoints.push_back(point);
    }
    for (auto point : actual.points())
    {
        actualPoints.push_back(point);
    }

    INFO(label);
    REQUIRE(actualVerbs.size() == expectedVerbs.size());
    for (size_t i = 0; i < expectedVerbs.size(); i++)
    {
        REQUIRE(actualVerbs[i] == expectedVerbs[i]);
    }
    REQUIRE(actualPoints.size() == expectedPoints.size());
    for (size_t i = 0; i < expectedPoints.size(); i++)
    {
        REQUIRE(actualPoints[i].x == Approx(expectedPoints[i].x));
        REQUIRE(actualPoints[i].y == Approx(expectedPoints[i].y));
    }
}
} // namespace

TEST_CASE("rounded rect matches Rectangle", "[rawpath]")
{
    struct Case
    {
        const char* label;
        float width, height, tl, tr, br, bl;
    };
    const Case cases[] = {
        {"square corners", 200.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {"uniform radius", 200.0f, 100.0f, 12.0f, 12.0f, 12.0f, 12.0f},
        {"per-corner radii", 200.0f, 100.0f, 4.0f, 8.0f, 16.0f, 32.0f},
        {"one rounded corner", 200.0f, 100.0f, 20.0f, 0.0f, 0.0f, 0.0f},
        // Clamps to half the shorter side.
        {"radius past the clamp",
         200.0f,
         100.0f,
         500.0f,
         500.0f,
         500.0f,
         500.0f},
        {"radius exactly at clamp", 200.0f, 100.0f, 50.0f, 50.0f, 50.0f, 50.0f},
        // Tiny radius still rounds: Path branches on the authored value, not
        // the clamped one.
        {"sub-pixel radius", 200.0f, 100.0f, 0.01f, 0.0f, 0.0f, 0.0f},
        {"tall", 40.0f, 400.0f, 10.0f, 0.0f, 10.0f, 0.0f},
        {"square", 100.0f, 100.0f, 25.0f, 25.0f, 25.0f, 25.0f},
        {"zero size", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {"zero size with radius", 0.0f, 0.0f, 10.0f, 10.0f, 10.0f, 10.0f},
        {"degenerate width", 0.0f, 100.0f, 5.0f, 5.0f, 5.0f, 5.0f},
        {"negative radius", 200.0f, 100.0f, -20.0f, -20.0f, -20.0f, -20.0f},
        {"one negative corner", 200.0f, 100.0f, -20.0f, 20.0f, 0.0f, 0.0f},
        {"negative past the clamp", 200.0f, 100.0f, -500.0f, 0.0f, 0.0f, 0.0f},
    };

    for (const auto& c : cases)
    {
        requireSamePath(viaRectangle(c.width, c.height, c.tl, c.tr, c.br, c.bl),
                        viaHelper(c.width, c.height, c.tl, c.tr, c.br, c.bl),
                        c.label);
    }
}

TEST_CASE("rounded rect honours an offset origin", "[rawpath]")
{
    // LayoutComponent draws at 0,0, but the helper takes an AABB so it must
    // translate with it.
    RawPath path;
    Path::addRoundedRect(path,
                         AABB{10.0f, 20.0f, 110.0f, 70.0f},
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f);
    auto bounds = path.bounds();
    REQUIRE(bounds.left() == Approx(10.0f));
    REQUIRE(bounds.top() == Approx(20.0f));
    REQUIRE(bounds.right() == Approx(110.0f));
    REQUIRE(bounds.bottom() == Approx(70.0f));
}

// updateRenderPath goes through ShapePaintPath, whose addPath() also prunes
// empty segments. Writing straight into mutableRawPath() skips that, so
// compare at the layer LayoutComponent actually uses.
TEST_CASE("rounded rect matches through ShapePaintPath", "[rawpath]")
{
    struct Case
    {
        const char* label;
        float width, height, tl, tr, br, bl;
    };
    const Case cases[] = {
        {"square corners", 200.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {"uniform radius", 200.0f, 100.0f, 12.0f, 12.0f, 12.0f, 12.0f},
        {"radius past the clamp",
         200.0f,
         100.0f,
         500.0f,
         500.0f,
         500.0f,
         500.0f},
        {"zero size", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {"zero size with radius", 0.0f, 0.0f, 10.0f, 10.0f, 10.0f, 10.0f},
        {"degenerate width", 0.0f, 100.0f, 5.0f, 5.0f, 5.0f, 5.0f},
        {"degenerate height", 200.0f, 0.0f, 5.0f, 5.0f, 5.0f, 5.0f},
    };

    for (const auto& c : cases)
    {
        ShapePaintPath oldWay;
        oldWay.rewind();
        oldWay.addPath(viaRectangle(c.width, c.height, c.tl, c.tr, c.br, c.bl));

        // Exactly what updateRenderPath does: build into a RawPath, then add
        // it through ShapePaintPath so empty segments are pruned.
        RawPath background;
        Path::addRoundedRect(background,
                             AABB{0.0f, 0.0f, c.width, c.height},
                             c.tl,
                             c.tr,
                             c.br,
                             c.bl);
        ShapePaintPath newWay;
        newWay.rewind();
        newWay.addPath(background);

        requireSamePath(*oldWay.rawPath(), *newWay.rawPath(), c.label);
    }
}
