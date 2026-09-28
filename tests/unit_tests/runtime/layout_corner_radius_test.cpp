#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_enums.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// layout_anim_bound.riv: a container with a custom layout animation holding
// six filled 100x100 layouts whose animation style is inherit.
static rive::LayoutComponent* findLayout(rive::Artboard* artboard,
                                         rive::LayoutAnimationStyle style)
{
    for (auto* layout : artboard->find<rive::LayoutComponent>())
    {
        if (!layout->is<rive::Artboard>() && layout->style() != nullptr &&
            layout->animationStyle() == style)
        {
            return layout;
        }
    }
    return nullptr;
}

static rive::RawPath roundedRect(rive::LayoutComponent* layout,
                                 float topLeft,
                                 float topRight,
                                 float bottomRight,
                                 float bottomLeft)
{
    rive::RawPath path;
    rive::Path::addRoundedRect(path,
                               rive::AABB::fromLTWH(0.0f,
                                                    0.0f,
                                                    layout->layoutWidth(),
                                                    layout->layoutHeight()),
                               topLeft,
                               topRight,
                               bottomRight,
                               bottomLeft);
    return path;
}

TEST_CASE("a corner radius change rebuilds only its layout's path", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_anim_bound.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);
    auto* layout =
        findLayout(artboard.get(), rive::LayoutAnimationStyle::inherit);
    REQUIRE(layout != nullptr);
    REQUIRE(layout->actualDirection() != rive::LayoutDirection::rtl);
    auto* style = layout->style();

    style->linkCornerRadius(true);
    style->cornerRadiusTL(12.0f);
    // Only the layout's own path is rebuilt; the artboard doesn't re-cascade
    // styles through the whole tree for it.
    CHECK_FALSE(artboard->hasDirt(rive::ComponentDirt::LayoutStyle));
    artboard->advance(0.0f);
    CHECK(*layout->localPath()->rawPath() ==
          roundedRect(layout, 12.0f, 12.0f, 12.0f, 12.0f));

    // Unlinking switches the path to the four separate radiuses.
    style->cornerRadiusTR(4.0f);
    style->cornerRadiusBR(8.0f);
    style->cornerRadiusBL(16.0f);
    style->linkCornerRadius(false);
    artboard->advance(0.0f);
    CHECK(*layout->localPath()->rawPath() ==
          roundedRect(layout, 12.0f, 4.0f, 8.0f, 16.0f));
}

// A radius change used to clear the layout's inherited interpolation until the
// next style cascade in updatePass. Keyframes apply before the layout advances,
// so a layout whose radius changed every frame never stepped its tween.
TEST_CASE("an inherited layout tween runs while its corner radius changes",
          "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_anim_bound.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);
    auto* container =
        findLayout(artboard.get(), rive::LayoutAnimationStyle::custom);
    auto* layout =
        findLayout(artboard.get(), rive::LayoutAnimationStyle::inherit);
    REQUIRE(container != nullptr);
    REQUIRE(layout != nullptr);

    // The file binds the container's interpolation time, which reads 0 while
    // unbound. Give it a one second linear tween for its children to inherit.
    container->style()->interpolationType(
        (uint8_t)rive::LayoutStyleInterpolation::linear);
    container->style()->interpolationTime(1.0f);
    artboard->advance(0.0f);
    REQUIRE(layout->interpolationTime() == 1.0f);
    REQUIRE(layout->layoutWidth() == 100.0f);

    // Shrink the layout while changing its radius every frame, as a keyed
    // radius would. Its width should pass through the tween.
    layout->width(50.0f);
    float midWidth = 100.0f;
    for (int i = 0; i < 5; i++)
    {
        layout->style()->cornerRadiusTL((float)(i + 1));
        artboard->advance(0.1f);
        float width = layout->layoutWidth();
        if (width > 50.0f && width < 100.0f)
        {
            midWidth = width;
        }
    }
    CHECK(midWidth < 100.0f);
    CHECK(midWidth > 50.0f);
}
