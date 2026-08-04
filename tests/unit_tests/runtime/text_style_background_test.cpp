#include "rive/math/math_types.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_selection_path.hpp"
#include "rive/text/text_style_background.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/no_op_renderer.hpp"

#include <catch.hpp>

using namespace rive;

TEST_CASE("selection path joins rects from multiple lines", "[text]")
{
    TextSelectionPath path(true, FillRule::evenOdd);
    std::vector<AABB> rects = {
        AABB(0.0f, 0.0f, 100.0f, 20.0f),
        AABB(0.0f, 20.0f, 60.0f, 40.0f),
    };
    path.update(rects, 0.0f);

    REQUIRE(path.fillRule() == FillRule::evenOdd);
    REQUIRE(!path.empty());
    // Touching lines of different widths merge into one contour.
    REQUIRE(path.numContours() == 1);
    AABB bounds = path.rawPath()->bounds();
    REQUIRE(bounds.minX == 0.0f);
    REQUIRE(bounds.minY == 0.0f);
    REQUIRE(bounds.maxX == 100.0f);
    REQUIRE(bounds.maxY == 40.0f);

    // No curves without corner rounding.
    for (auto verb : path.rawPath()->verbs())
    {
        REQUIRE(verb != PathVerb::cubic);
    }
}

TEST_CASE("selection path rounds corners with clamped radius", "[text]")
{
    TextSelectionPath path(true, FillRule::evenOdd);
    std::vector<AABB> rects = {
        AABB(0.0f, 0.0f, 100.0f, 20.0f),
        AABB(0.0f, 20.0f, 60.0f, 40.0f),
    };
    path.update(rects, 6.0f);

    REQUIRE(!path.empty());
    size_t cubics = 0;
    for (auto verb : path.rawPath()->verbs())
    {
        if (verb == PathVerb::cubic)
        {
            cubics++;
        }
    }
    // One rounded corner per contour vertex: 4 outer box corners plus the
    // concave step between the two line widths (2 vertices).
    REQUIRE(cubics == 6);

    // Rounding must stay within the rect union.
    AABB bounds = path.rawPath()->bounds();
    REQUIRE(bounds.minX >= 0.0f);
    REQUIRE(bounds.minY >= 0.0f);
    REQUIRE(bounds.maxX <= 100.0f);
    REQUIRE(bounds.maxY <= 40.0f);
}

TEST_CASE("selection path keeps disjoint lines as separate contours", "[text]")
{
    TextSelectionPath path(true, FillRule::evenOdd);
    std::vector<AABB> rects = {
        AABB(0.0f, 0.0f, 100.0f, 20.0f),
        AABB(0.0f, 30.0f, 60.0f, 50.0f),
    };
    path.update(rects, 4.0f);
    REQUIRE(path.numContours() == 2);
}

TEST_CASE("selection path rewinds between updates", "[text]")
{
    TextSelectionPath path(true, FillRule::evenOdd);
    std::vector<AABB> rects = {AABB(0.0f, 0.0f, 100.0f, 20.0f)};
    path.update(rects, 0.0f);
    REQUIRE(path.numContours() == 1);
    path.update(rects, 0.0f);
    REQUIRE(path.numContours() == 1);

    path.update(Span<AABB>(nullptr, 0), 0.0f);
    REQUIRE(path.empty());
}

TEST_CASE("editor-exported text style background renders at runtime", "[text]")
{
    auto file = ReadRiveFile("assets/text_style_background.riv");
    auto artboard = file->artboard();

    auto backgrounds = artboard->find<rive::TextStyleBackground>();
    REQUIRE(backgrounds.size() == 1);
    auto background = backgrounds[0];
    REQUIRE(background->cornerRadius() == 8.0f);
    REQUIRE(background->parent()->is<rive::TextStylePaint>());
    REQUIRE(background->parent()->as<rive::TextStylePaint>()->background() ==
            background);
    // Fill plus stroke, drawn in child order.
    REQUIRE(background->shapePaints().size() == 2);

    artboard->advance(0.0f);

    // Three contiguous lines merge into one rounded contour.
    ShapePaintPath* path = background->localPath();
    REQUIRE(!path->empty());
    REQUIRE(path->numContours() == 1);
    bool hasCubics = false;
    for (auto verb : path->rawPath()->verbs())
    {
        if (verb == PathVerb::cubic)
        {
            hasCubics = true;
        }
    }
    REQUIRE(hasCubics);

    // Background bounds contain the text bounds vertically and horizontally.
    auto textObjects = artboard->find<rive::Text>();
    REQUIRE(textObjects.size() == 1);
    AABB textBounds = textObjects[0]->localBounds();
    AABB pathBounds = path->rawPath()->bounds();
    REQUIRE(pathBounds.width() > 0.0f);
    REQUIRE(pathBounds.width() <= textBounds.width() + 1.0f);
    REQUIRE(pathBounds.height() <= textBounds.height() + 1.0f);

    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);
}

TEST_CASE("text style background is a shape paint container", "[text]")
{
    TextStyleBackground background;
    REQUIRE(ShapePaintContainer::from(&background) == &background);
    REQUIRE(background.localPath() != nullptr);
    REQUIRE(background.localClockwisePath() == background.localPath());
    REQUIRE(background.localPath()->fillRule() == FillRule::evenOdd);
    REQUIRE(background.cornerRadius() == 0.0f);
}
