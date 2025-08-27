#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/math/transform_components.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("follow path constraint updates world transform", "[file]")
{
    auto file = ReadRiveFile("assets/follow_path.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::TransformComponent>("target") != nullptr);
    auto target = artboard->find<rive::TransformComponent>("target");

    REQUIRE(artboard->find<rive::TransformComponent>("rect") != nullptr);
    auto rectangle = artboard->find<rive::TransformComponent>("rect");

    artboard->advance(0.0f);

    auto targetComponents = target->worldTransform().decompose();
    auto rectComponents = rectangle->worldTransform().decompose();
    REQUIRE(targetComponents.x() == rectComponents.x());
    REQUIRE(targetComponents.y() == rectComponents.y());
}

TEST_CASE("follow path with 0 opacity constraint updates world transform",
          "[file]")
{
    auto file = ReadRiveFile("assets/follow_path_with_0_opacity.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::TransformComponent>("target") != nullptr);
    auto target = artboard->find<rive::TransformComponent>("target");

    REQUIRE(artboard->find<rive::TransformComponent>("rect") != nullptr);
    auto rectangle = artboard->find<rive::TransformComponent>("rect");

    artboard->advance(0.0f);

    auto targetComponents = target->worldTransform().decompose();
    auto rectComponents = rectangle->worldTransform().decompose();
    REQUIRE(targetComponents.x() == rectComponents.x());
    REQUIRE(targetComponents.y() == rectComponents.y());
}

TEST_CASE(
    "follow path constraint with path at 0 opacity updates world transform",
    "[file]")
{
    auto file = ReadRiveFile("assets/follow_path_path_0_opacity.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::TransformComponent>("target") != nullptr);
    auto target = artboard->find<rive::TransformComponent>("target");

    REQUIRE(artboard->find<rive::TransformComponent>("rect") != nullptr);
    auto rectangle = artboard->find<rive::TransformComponent>("rect");

    artboard->advance(0.0f);

    auto targetComponents = target->worldTransform().decompose();
    auto rectComponents = rectangle->worldTransform().decompose();
    REQUIRE(targetComponents.x() == rectComponents.x());
    REQUIRE(targetComponents.y() == rectComponents.y());
}
