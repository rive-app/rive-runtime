#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_enums.hpp"
#include "rive/math/aabb.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "utils/no_op_renderer.hpp"
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

TEST_CASE("moving a layout keeps its local render path", "[layout]")
{
    // 0 draws a plain rect; 50 rounds the 100x100 layout into a circle, whose
    // zero-length sides get pruned.
    auto radius = GENERATE(0.0f, 12.0f, 50.0f);
    auto file = ReadRiveFile("assets/layout/layout_anim_bound.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto* container =
        findLayout(artboard.get(), rive::LayoutAnimationStyle::custom);
    auto* layout =
        findLayout(artboard.get(), rive::LayoutAnimationStyle::inherit);
    REQUIRE(container != nullptr);
    REQUIRE(layout != nullptr);
    layout->style()->linkCornerRadius(true);
    layout->style()->cornerRadiusTL(radius);

    rive::NoOpRenderer renderer;
    artboard->advance(0.0f);
    artboard->draw(&renderer);
    auto* local = layout->localPath();
    auto* world = layout->worldPath();
    REQUIRE(local != nullptr);
    REQUIRE(world != nullptr);
    // The fill draws the local path, so drawing gave it a render path.
    REQUIRE(local->hasRenderPath());
    auto worldBounds = world->rawPath()->bounds();

    // Padding the container shifts the layout without changing its shape: only
    // the world path, which bakes in the transform, needs new geometry. The
    // local render path, and whatever the renderer cached for it, stays.
    auto* containerStyle = container->style();
    containerStyle->paddingLeft(containerStyle->paddingLeft() + 10.0f);
    artboard->advance(0.0f);
    CHECK(local->hasRenderPath());
    auto movedBounds = world->rawPath()->bounds();
    CHECK(movedBounds.left() != worldBounds.left());
    CHECK(movedBounds.width() == Approx(worldBounds.width()));
}

// A layout tween resizes the layout while marking only its world transform
// dirty, so the local path has to notice the size change on its own.
TEST_CASE("a tweening layout's local path follows its size", "[layout]")
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
    REQUIRE(layout->layoutWidth() == 100.0f);

    rive::NoOpRenderer renderer;
    layout->width(50.0f);
    bool sawMidTween = false;
    for (int i = 0; i < 12; i++)
    {
        artboard->advance(0.1f);
        artboard->draw(&renderer);
        float width = layout->layoutWidth();
        sawMidTween = sawMidTween || (width > 50.0f && width < 100.0f);
        CHECK(layout->localPath()->rawPath()->bounds() ==
              rive::AABB::fromLTWH(0.0f, 0.0f, width, layout->layoutHeight()));
    }
    CHECK(sawMidTween);
    CHECK(layout->layoutWidth() == 50.0f);
}
