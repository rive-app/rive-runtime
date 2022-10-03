#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/bones/bone.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/math/transform_components.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("rotation constraint updates world transform", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/rotation_constraint.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::TransformComponent>("target") != nullptr);
    auto target = artboard->find<rive::TransformComponent>("target");

    REQUIRE(artboard->find<rive::TransformComponent>("rect") != nullptr);
    auto rectangle = artboard->find<rive::TransformComponent>("rect");

    artboard->advance(0.0f);
    auto targetComponents = target->worldTransform().decompose();
    auto rectComponents = rectangle->worldTransform().decompose();

    REQUIRE(targetComponents.rotation() == rectComponents.rotation());
}
