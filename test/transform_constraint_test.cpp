#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/bones/bone.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("transform constraint updates world transform", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/transform_constraint.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::TransformComponent>("Target") != nullptr);
    auto target = artboard->find<rive::TransformComponent>("Target");

    REQUIRE(artboard->find<rive::TransformComponent>("Rectangle") != nullptr);
    auto rectangle = artboard->find<rive::TransformComponent>("Rectangle");

    artboard->advance(0.0f);

    // Expect the transform constraint to have placed the shape in the same
    // exact world transform as the target.
    REQUIRE(aboutEqual(target->worldTransform(), rectangle->worldTransform()));
}
