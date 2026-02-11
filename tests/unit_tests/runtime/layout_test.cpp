#include "rive/animation/state_machine_instance.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_enums.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/text/text.hpp"
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("LayoutComponent FlexDirection row", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_horizontal.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent1") !=
            nullptr);
    auto target1 = artboard->find<rive::LayoutComponent>("LayoutComponent1");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent2") !=
            nullptr);
    auto target2 = artboard->find<rive::LayoutComponent>("LayoutComponent2");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent3") !=
            nullptr);
    auto target3 = artboard->find<rive::LayoutComponent>("LayoutComponent3");

    artboard->advance(0.0f);
    auto target1Components = target1->worldTransform().decompose();
    auto target2Components = target2->worldTransform().decompose();
    auto target3Components = target3->worldTransform().decompose();

    auto style = target1->style();
    REQUIRE(style != nullptr);
    REQUIRE(style->flexDirection() == YGFlexDirectionRow);

    REQUIRE(target1Components.x() == 0);
    REQUIRE(target2Components.x() == 100);
    REQUIRE(target3Components.x() == 200);
    REQUIRE(target1Components.y() == 0);
    REQUIRE(target2Components.y() == 0);
    REQUIRE(target3Components.y() == 0);
}

TEST_CASE("LayoutComponent FlexDirection column", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_vertical.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent1") !=
            nullptr);
    auto target1 = artboard->find<rive::LayoutComponent>("LayoutComponent1");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent2") !=
            nullptr);
    auto target2 = artboard->find<rive::LayoutComponent>("LayoutComponent2");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent3") !=
            nullptr);
    auto target3 = artboard->find<rive::LayoutComponent>("LayoutComponent3");

    artboard->advance(0.0f);
    auto target1Components = target1->worldTransform().decompose();
    auto target2Components = target2->worldTransform().decompose();
    auto target3Components = target3->worldTransform().decompose();

    REQUIRE(target1Components.x() == 0);
    REQUIRE(target2Components.x() == 0);
    REQUIRE(target3Components.x() == 0);
    REQUIRE(target1Components.y() == 0);
    REQUIRE(target2Components.y() == 100);
    REQUIRE(target3Components.y() == 200);
}

TEST_CASE("LayoutComponent FlexDirection row with gap", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_horizontal_gaps.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent1") !=
            nullptr);
    auto target1 = artboard->find<rive::LayoutComponent>("LayoutComponent1");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent2") !=
            nullptr);
    auto target2 = artboard->find<rive::LayoutComponent>("LayoutComponent2");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent3") !=
            nullptr);
    auto target3 = artboard->find<rive::LayoutComponent>("LayoutComponent3");

    artboard->advance(0.0f);
    auto target1Components = target1->worldTransform().decompose();
    auto target2Components = target2->worldTransform().decompose();
    auto target3Components = target3->worldTransform().decompose();

    REQUIRE(target1Components.x() == 0);
    REQUIRE(target2Components.x() == 110);
    REQUIRE(target3Components.x() == 220);
    REQUIRE(target1Components.y() == 0);
    REQUIRE(target2Components.y() == 0);
    REQUIRE(target3Components.y() == 0);
}

TEST_CASE("LayoutComponent FlexDirection row with wrap", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_horizontal_wrap.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent6") !=
            nullptr);
    auto target = artboard->find<rive::LayoutComponent>("LayoutComponent6");

    artboard->advance(0.0f);
    auto targetComponents = target->worldTransform().decompose();

    REQUIRE(targetComponents.x() == 0);
    REQUIRE(targetComponents.y() == 100);
}

TEST_CASE("LayoutComponent Center using alignItems and justifyContent",
          "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_center.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent1") !=
            nullptr);
    auto target = artboard->find<rive::LayoutComponent>("LayoutComponent1");

    artboard->advance(0.0f);
    auto targetComponents = target->worldTransform().decompose();

    REQUIRE(targetComponents.x() == 200);
    REQUIRE(targetComponents.y() == 200);
}

TEST_CASE("LayoutComponent with intrinsic size gets measured correctly",
          "[layout]")
{
    auto file = ReadRiveFile("assets/layout/measure_tests.riv");

    auto artboard = file->artboard("hi");

    REQUIRE(artboard->find<rive::LayoutComponent>("TextLayout") != nullptr);
    REQUIRE(artboard->find<rive::Text>("HiText") != nullptr);

    artboard->advance(0.0f);

    auto text = artboard->find<rive::Text>("HiText");
    auto bounds = text->localBounds();
    REQUIRE(bounds.left() == 0);
    REQUIRE(bounds.top() == 0);
    REQUIRE(bounds.width() == 62.48047f);
    REQUIRE(bounds.height() == 72.62695f);
}

TEST_CASE("LayoutComponent Padding Px", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_complex1.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutLeft") != nullptr);
    auto parent = artboard->find<rive::LayoutComponent>("LayoutLeft");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutLeftChild1") !=
            nullptr);
    auto child1 = artboard->find<rive::LayoutComponent>("LayoutLeftChild1");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutLeftChild2") !=
            nullptr);
    auto child2 = artboard->find<rive::LayoutComponent>("LayoutLeftChild2");

    artboard->advance(0.0f);

    auto style = parent->style();
    REQUIRE(style != nullptr);
    REQUIRE(style->paddingLeft() == 20);
    REQUIRE(style->paddingLeftUnits() == YGUnitPoint);
    REQUIRE(style->paddingRight() == 20);
    REQUIRE(style->paddingRightUnits() == YGUnitPoint);
    REQUIRE(style->paddingTop() == 20);
    REQUIRE(style->paddingTopUnits() == YGUnitPoint);
    REQUIRE(style->paddingBottom() == 20);
    REQUIRE(style->paddingBottomUnits() == YGUnitPoint);

    auto parentComponents = parent->worldTransform().decompose();
    auto child1Components = child1->worldTransform().decompose();
    auto child2Components = child2->worldTransform().decompose();

    REQUIRE(parentComponents.x() == 0);
    REQUIRE(child1Components.x() == 20);
    REQUIRE(child2Components.x() == 130);
    REQUIRE(parentComponents.y() == 0);
    REQUIRE(child1Components.y() == 20);
    REQUIRE(child2Components.y() == 20);
}

TEST_CASE("LayoutComponent Margin Px", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_complex1.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutRight") != nullptr);
    auto parent = artboard->find<rive::LayoutComponent>("LayoutRight");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutRightChild1") !=
            nullptr);
    auto child1 = artboard->find<rive::LayoutComponent>("LayoutRightChild1");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutRightChild2") !=
            nullptr);
    auto child2 = artboard->find<rive::LayoutComponent>("LayoutRightChild2");

    artboard->advance(0.0f);

    auto style1 = child1->style();
    REQUIRE(style1 != nullptr);
    REQUIRE(style1->marginLeft() == 10);
    REQUIRE(style1->marginLeftUnits() == YGUnitPoint);
    REQUIRE(style1->marginRight() == 10);
    REQUIRE(style1->marginRightUnits() == YGUnitPoint);
    REQUIRE(style1->marginTop() == 10);
    REQUIRE(style1->marginTopUnits() == YGUnitPoint);
    REQUIRE(style1->marginBottom() == 10);
    REQUIRE(style1->marginBottomUnits() == YGUnitPoint);
    REQUIRE(style1->alignmentType() == rive::LayoutAlignmentType::center);
    REQUIRE(style1->flexWrap() == YGWrapNoWrap);

    auto style2 = child2->style();
    REQUIRE(style2 != nullptr);
    REQUIRE(style2->marginLeft() == 5);
    REQUIRE(style2->marginLeftUnits() == YGUnitPercent);
    REQUIRE(style2->marginRight() == 5);
    REQUIRE(style2->marginRightUnits() == YGUnitPercent);
    REQUIRE(style2->marginTop() == 5);
    REQUIRE(style2->marginTopUnits() == YGUnitPercent);
    REQUIRE(style2->marginBottom() == 5);
    REQUIRE(style2->marginBottomUnits() == YGUnitPercent);
    REQUIRE(style2->alignmentType() == rive::LayoutAlignmentType::topLeft);
    REQUIRE(style2->flexWrap() == YGWrapWrap);

    auto parentComponents = parent->worldTransform().decompose();
    auto child1Components = child1->worldTransform().decompose();
    auto child2Components = child2->worldTransform().decompose();

    REQUIRE(parentComponents.x() == 250);
    REQUIRE(child1Components.x() == 285);
    REQUIRE(child2Components.x() == 285);
    REQUIRE(parentComponents.y() == 0);
    REQUIRE(child1Components.y() == 35);
    REQUIRE(child2Components.y() == 215);
}

TEST_CASE("LayoutComponent Corner Radius", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_complex1.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutLeftChild1") !=
            nullptr);
    auto child1 = artboard->find<rive::LayoutComponent>("LayoutLeftChild1");

    artboard->advance(0.0f);

    auto style = child1->style();
    REQUIRE(style != nullptr);
    REQUIRE(style->cornerRadiusTL() == 15);
    REQUIRE(style->cornerRadiusTR() == 15);
    REQUIRE(style->cornerRadiusBL() == 15);
    REQUIRE(style->cornerRadiusBR() == 15);
}

TEST_CASE("LayoutComponent Direction", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_direction.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("Layout1") != nullptr);
    auto target1 = artboard->find<rive::LayoutComponent>("Layout1");

    REQUIRE(artboard->find<rive::LayoutComponent>("Layout2") != nullptr);
    auto target2 = artboard->find<rive::LayoutComponent>("Layout2");

    REQUIRE(artboard->find<rive::LayoutComponent>("Layout3") != nullptr);
    auto target3 = artboard->find<rive::LayoutComponent>("Layout3");

    REQUIRE(artboard->find<rive::Text>("SampleText") != nullptr);
    auto text = artboard->find<rive::Text>("SampleText");

    artboard->advance(0.0f);
    auto target1Components = target1->worldTransform().decompose();
    auto target2Components = target2->worldTransform().decompose();
    auto target3Components = target3->worldTransform().decompose();

    REQUIRE(target1Components.x() == 200);
    REQUIRE(target2Components.x() == 100);
    REQUIRE(target3Components.x() == 0);
    REQUIRE(target1->actualDirection() == rive::LayoutDirection::rtl);
    REQUIRE(target2->actualDirection() == rive::LayoutDirection::rtl);
    REQUIRE(target3->actualDirection() == rive::LayoutDirection::rtl);
    REQUIRE(text->align() == rive::TextAlign::right);
}

TEST_CASE("LayoutComponent forcedWidth/Height dirt test", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_complex1.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutLeftChild1") !=
            nullptr);
    auto layout = artboard->find<rive::LayoutComponent>("LayoutLeftChild1");
    REQUIRE(std::isnan(layout->forcedWidth()));
    REQUIRE(std::isnan(layout->forcedHeight()));
    layout->forcedWidth(100);
    layout->forcedHeight(150);
    REQUIRE(layout->forcedWidth() == 100.0f);
    REQUIRE(layout->forcedHeight() == 150.0f);
    // forcedWidth/Height adds LayoutStyle dirt
    REQUIRE(layout->hasDirt(rive::ComponentDirt::LayoutStyle) == true);
    artboard->advance(0.0f);
    // Advancing clears dirt
    REQUIRE(layout->hasDirt(rive::ComponentDirt::LayoutStyle) == false);
    layout->forcedWidth(100);
    layout->forcedHeight(150);
    // Setting the same values should result in no added dirt
    REQUIRE(layout->hasDirt(rive::ComponentDirt::LayoutStyle) == false);
}

TEST_CASE("LayoutComponent Alignment", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_alignment.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutContainer") !=
            nullptr);
    auto container = artboard->find<rive::LayoutComponent>("LayoutContainer");

    REQUIRE(artboard->find<rive::LayoutComponent>("Layout1") != nullptr);
    auto layout1 = artboard->find<rive::LayoutComponent>("Layout1");

    REQUIRE(artboard->find<rive::LayoutComponent>("Layout2") != nullptr);
    auto layout2 = artboard->find<rive::LayoutComponent>("Layout2");

    REQUIRE(artboard->find<rive::LayoutComponent>("Layout3") != nullptr);
    auto layout3 = artboard->find<rive::LayoutComponent>("Layout3");

    auto style = container->style();
    REQUIRE(style != nullptr);

    // LayoutAlignmentType::spaceBetweenStart
    container->style()->layoutAlignmentType(9);

    artboard->advance(0.0f);
    auto target1Components = layout1->worldTransform().decompose();
    auto target2Components = layout2->worldTransform().decompose();
    auto target3Components = layout3->worldTransform().decompose();

    REQUIRE(target1Components.x() == 0);
    REQUIRE(target2Components.x() == 200);
    REQUIRE(target3Components.x() == 400);
    REQUIRE(target1Components.y() == 0);
    REQUIRE(target2Components.y() == 0);
    REQUIRE(target3Components.y() == 0);

    // LayoutAlignmentType::spaceBetweenCenter
    container->style()->layoutAlignmentType(10);

    artboard->advance(0.0f);
    target1Components = layout1->worldTransform().decompose();
    target2Components = layout2->worldTransform().decompose();
    target3Components = layout3->worldTransform().decompose();

    REQUIRE(target1Components.x() == 0);
    REQUIRE(target2Components.x() == 200);
    REQUIRE(target3Components.x() == 400);
    REQUIRE(target1Components.y() == 200);
    REQUIRE(target2Components.y() == 200);
    REQUIRE(target3Components.y() == 200);

    // YGFlexDirectionColumn
    container->style()->flexDirectionValue(0);

    artboard->advance(0.0f);
    target1Components = layout1->worldTransform().decompose();
    target2Components = layout2->worldTransform().decompose();
    target3Components = layout3->worldTransform().decompose();

    REQUIRE(target1Components.x() == 200);
    REQUIRE(target2Components.x() == 200);
    REQUIRE(target3Components.x() == 200);
    REQUIRE(target1Components.y() == 0);
    REQUIRE(target2Components.y() == 200);
    REQUIRE(target3Components.y() == 400);

    // LayoutAlignmentType::spaceBetweenEnd
    container->style()->layoutAlignmentType(11);

    artboard->advance(0.0f);
    target1Components = layout1->worldTransform().decompose();
    target2Components = layout2->worldTransform().decompose();
    target3Components = layout3->worldTransform().decompose();

    REQUIRE(target1Components.x() == 400);
    REQUIRE(target2Components.x() == 400);
    REQUIRE(target3Components.x() == 400);
    REQUIRE(target1Components.y() == 0);
    REQUIRE(target2Components.y() == 200);
    REQUIRE(target3Components.y() == 400);
}

TEST_CASE("Prevent Margin Pct on Artboard", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/artboard_percent_margin.riv");

    auto artboard = file->artboard();

    artboard->advance(0.0f);

    REQUIRE(artboard->layoutWidth() == 501.0f);
    REQUIRE(artboard->layoutHeight() == 512.0f);
}

TEST_CASE("Multiple layout collapsing and soloing in hierarchy.", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/collapsing_elements.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(4.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("collapsing_elements"));
}

TEST_CASE("Animating layout display", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_display.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.5f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_display"));
}

TEST_CASE("Layout background & foreground shape paints.", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_paint.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(2.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_paint"));
}

TEST_CASE("Layout animation time databound", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_anim_bound.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    auto stateMachine = artboard->stateMachineAt(0);

    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 32;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_anim_bound"));
}

TEST_CASE("Layout aspect ratio", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_aspect_ratio.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    auto stateMachine = artboard->stateMachineAt(0);

    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 32;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_aspect_ratio"));
}