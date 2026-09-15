#include "rive/animation/state_machine_instance.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_enums.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/text/text.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
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

TEST_CASE("LayoutComponent Corner Radius Link Change", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_complex1.riv");

    auto artboard = file->artboard();

    auto child1 = artboard->find<rive::LayoutComponent>("LayoutLeftChild1");
    REQUIRE(child1 != nullptr);
    auto style = child1->style();
    REQUIRE(style != nullptr);

    // localPath() mirrors the background path updateRenderPath builds, and is
    // the closest public view of it.
    auto capture = [&]() {
        artboard->advance(0.0f);
        std::vector<rive::Vec2D> points;
        auto path = child1->localPath();
        if (path != nullptr)
        {
            for (auto point : path->rawPath()->points())
            {
                points.push_back(point);
            }
        }
        return points;
    };

    // The shape linking should produce: every corner following TL.
    style->linkCornerRadius(false);
    style->cornerRadiusTL(4.0f);
    style->cornerRadiusTR(4.0f);
    style->cornerRadiusBL(4.0f);
    style->cornerRadiusBR(4.0f);
    auto uniform = capture();
    REQUIRE(!uniform.empty());

    // Same component with the corners pulled apart, still unlinked.
    style->cornerRadiusTR(12.0f);
    style->cornerRadiusBL(20.0f);
    style->cornerRadiusBR(28.0f);
    auto distinct = capture();
    REQUIRE(distinct.size() == uniform.size());

    // Linking has to rebuild the background from TL alone. The flag is
    // bindable, so this can happen mid-playback; without
    // LayoutComponentStyle::linkCornerRadiusChanged marking the style dirty
    // nothing rebuilds and the path stays on the distinct corners.
    style->linkCornerRadius(true);
    auto linked = capture();

    REQUIRE(linked.size() == uniform.size());
    for (size_t i = 0; i < uniform.size(); i++)
    {
        REQUIRE(linked[i].x == Approx(uniform[i].x));
        REQUIRE(linked[i].y == Approx(uniform[i].y));
    }
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

TEST_CASE("Layout animation nested artboards", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/layout/layout_animation_nested.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 72;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_anim_nested"));
}

TEST_CASE("Layout animation artboard component list", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/layout/layout_animation_component_list.riv",
                     &silver);

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

    int frames = 72;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_anim_component_list"));
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

TEST_CASE("Layout fixed/fill scale type round trip preserves units", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_fixed_fill.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->bindViewModelInstance(viewModelInstance);

    stateMachine->advanceAndApply(0.0f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto boolProp = viewModelInstance->propertyValue("booleanProperty")
                        ->as<rive::ViewModelInstanceBoolean>();
    REQUIRE(boolProp != nullptr);
    boolProp->propertyValue(true);

    int frames = 15;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_fixed_fill"));
}

TEST_CASE("Top-level hug artboard computes size from children", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_hug_artboard.riv", &silver);
    auto artboard = file->artboardNamed("HugArtboard");
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    artboard->advance(0.0f);

    silver.frameSize(artboard->layoutWidth(), artboard->layoutHeight());

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Hug artboard should derive its size from children, not its
    // authored width/height.
    REQUIRE(artboard->layoutWidth() == Approx(302.63f).margin(0.01f));
    REQUIRE(artboard->layoutHeight() == Approx(100.0f).margin(0.01f));

    CHECK(silver.matches("layout_hug_artboard"));
}
// Asset generated by gen_layout_fixtures.py: a 200x200 flex container with
// asymmetric padding (10/20/30/40) + margins and a single fill child. Padding
// insets the fill child, exercising the padding/margin (and units) getters in
// LayoutComponent::applyStyle and the shared LayoutSizingStyle base.
TEST_CASE("padding insets a fill child", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/styled_flex.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    // The child's parent is the container (a non-artboard LayoutComponent);
    // the container's parent is the artboard.
    rive::LayoutComponent* child = nullptr;
    for (auto* lc : artboard->find<rive::LayoutComponent>())
    {
        if (lc->is<rive::Artboard>() || lc->style() == nullptr)
        {
            continue;
        }
        auto* p = lc->parent();
        if (p != nullptr && p->is<rive::LayoutComponent>() &&
            !p->is<rive::Artboard>())
        {
            child = lc;
        }
    }
    REQUIRE(child != nullptr);
    // Yoga positions are parent-relative, so the fill child sits at the
    // container's padding offset with the padding removed from its size.
    REQUIRE(child->layoutX() == Approx(10.0f));       // paddingLeft
    REQUIRE(child->layoutY() == Approx(20.0f));       // paddingTop
    REQUIRE(child->layoutWidth() == Approx(160.0f));  // 200 - 10 - 30
    REQUIRE(child->layoutHeight() == Approx(140.0f)); // 200 - 20 - 40
}

TEST_CASE("Layout occluded by rectangle pointer test", "[silver]")
{
    // This tests two semi overlapping component, above is a layout and below is
    // an opaque rectangle
    // It has two toggling squares from red to green every time the pointer
    // triggers Top square is layout, bottom square is rectangle
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout_order_pointer_test.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    // Frame 0
    artboard->draw(renderer.get());

    // Frame 1
    // This clicks on layout but not on rectangle
    // The layout hit point is on a child of the layout with the listener
    // Top toggles top to green, bottom stays red
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(25.0f, 25.0f));
    stateMachine->pointerUp(rive::Vec2D(25.0f, 25.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Frame 2
    // This clicks both on layout and rectangle
    // The layout hit point is on a child of the layout with the listener
    // Top toggles top to red, toggles bottom to green
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(60.0f, 60.0f));
    stateMachine->pointerUp(rive::Vec2D(60.0f, 60.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Frame 3
    // This clicks both on layout and rectangle
    // The layout hit point is direct on the layout with the listener
    // Top toggles top to green, toggles bottom to red
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(150.0f, 150.0f));
    stateMachine->pointerUp(rive::Vec2D(150.0f, 150.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Frame 4
    // This clicks both on layout and rectangle
    // The layout hit point is on a child rectangle beyond the layout area, thus
    // not triggering the layout listener
    // Top stays green, toggles bottom to green
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(300.0f, 300.0f));
    stateMachine->pointerUp(rive::Vec2D(300.0f, 300.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Frame 5
    // This clicks only on the rectangle
    // Top stays green, toggles bottom to red
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(450.0f, 450.0f));
    stateMachine->pointerUp(rive::Vec2D(450.0f, 450.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("layout_order_pointer_test"));
}

TEST_CASE("Layout animation interrupted mid animation", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/layout_animation_transition_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

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

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(2.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_animation_transition_test"));
}