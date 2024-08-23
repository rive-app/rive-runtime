#include <rive/bones/skin.hpp>
#include <rive/bones/tendon.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/path_vertex.hpp>
#include <rive/shapes/points_path.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("bound bones load correctly", "[bones]")
{
    auto file = ReadRiveFile("../../test/assets/off_road_car.riv");

    auto node = file->artboard()->find("transmission_front_testing");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Shape>());
    REQUIRE(node->as<rive::Shape>()->paths().size() == 1);
    auto path = node->as<rive::Shape>()->paths()[0];
    REQUIRE(path->is<rive::PointsPath>());
    auto pointsPath = path->as<rive::PointsPath>();
    REQUIRE(pointsPath->skin() != nullptr);
    REQUIRE(pointsPath->skin()->tendons().size() == 2);
    REQUIRE(pointsPath->skin()->tendons()[0]->bone() != nullptr);
    REQUIRE(pointsPath->skin()->tendons()[1]->bone() != nullptr);

    for (auto vertex : path->vertices())
    {
        REQUIRE(vertex->weight() != nullptr);
    }

    // Ok seems like bones are set up ok.
}
