#include <rive/math/transform_components.hpp>
#include <rive/shapes/rectangle.hpp>
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("LayoutComponent FlexDirection row", "[layout]")
{
    auto file = ReadRiveFile("../../test/assets/layout/layout_horizontal.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent1") != nullptr);
    auto target1 = artboard->find<rive::LayoutComponent>("LayoutComponent1");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent2") != nullptr);
    auto target2 = artboard->find<rive::LayoutComponent>("LayoutComponent2");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent3") != nullptr);
    auto target3 = artboard->find<rive::LayoutComponent>("LayoutComponent3");

    artboard->advance(0.0f);
    auto target1Components = target1->worldTransform().decompose();
    auto target2Components = target2->worldTransform().decompose();
    auto target3Components = target3->worldTransform().decompose();

    REQUIRE(target1Components.x() == 0);
    REQUIRE(target2Components.x() == 100);
    REQUIRE(target3Components.x() == 200);
    REQUIRE(target1Components.y() == 0);
    REQUIRE(target2Components.y() == 0);
    REQUIRE(target3Components.y() == 0);
}

TEST_CASE("LayoutComponent FlexDirection column", "[layout]")
{
    auto file = ReadRiveFile("../../test/assets/layout/layout_vertical.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent1") != nullptr);
    auto target1 = artboard->find<rive::LayoutComponent>("LayoutComponent1");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent2") != nullptr);
    auto target2 = artboard->find<rive::LayoutComponent>("LayoutComponent2");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent3") != nullptr);
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
    auto file = ReadRiveFile("../../test/assets/layout/layout_horizontal_gaps.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent1") != nullptr);
    auto target1 = artboard->find<rive::LayoutComponent>("LayoutComponent1");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent2") != nullptr);
    auto target2 = artboard->find<rive::LayoutComponent>("LayoutComponent2");

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent3") != nullptr);
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
    auto file = ReadRiveFile("../../test/assets/layout/layout_horizontal_wrap.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent6") != nullptr);
    auto target = artboard->find<rive::LayoutComponent>("LayoutComponent6");

    artboard->advance(0.0f);
    auto targetComponents = target->worldTransform().decompose();

    REQUIRE(targetComponents.x() == 0);
    REQUIRE(targetComponents.y() == 100);
}

TEST_CASE("LayoutComponent Center using alignItems and justifyContent", "[layout]")
{
    auto file = ReadRiveFile("../../test/assets/layout/layout_center.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutComponent1") != nullptr);
    auto target = artboard->find<rive::LayoutComponent>("LayoutComponent1");

    artboard->advance(0.0f);
    auto targetComponents = target->worldTransform().decompose();

    REQUIRE(targetComponents.x() == 200);
    REQUIRE(targetComponents.y() == 200);
}