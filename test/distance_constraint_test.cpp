#include <rive/file.hpp>
#include <rive/constraints/distance_constraint.hpp>
#include <rive/node.hpp>
#include <rive/math/vec2d.hpp>
#include <rive/shapes/shape.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("distance constraints moves items as expected", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/distance_constraint.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::Shape>("A") != nullptr);
    auto a = artboard->find<rive::Shape>("A");

    REQUIRE(artboard->find<rive::Shape>("B") != nullptr);
    auto b = artboard->find<rive::Shape>("B");

    REQUIRE(a->constraints().size() == 1);
    REQUIRE(a->constraints()[0]->is<rive::DistanceConstraint>());

    auto distanceConstraint = a->constraints()[0]->as<rive::DistanceConstraint>();
    REQUIRE(distanceConstraint->modeValue() == 1);

    b->x(259.31f);
    b->y(137.87f);
    artboard->advance(0.0f);

    rive::Vec2D at = a->worldTranslation();
    rive::Vec2D expectedTranslation(259.2808837890625f, 62.87000274658203f);
    REQUIRE(rive::Vec2D::distance(at, expectedTranslation) < 0.001f);
}
