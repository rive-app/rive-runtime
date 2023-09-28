#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/image.hpp>
#include <rive/shapes/mesh.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/relative_local_asset_loader.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("image with mesh loads correctly", "[mesh]")
{
    auto file = ReadRiveFile("../../test/assets/tape.riv");

    auto node = file->artboard()->find("Tape body.png");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Image>());
    auto tape = node->as<rive::Image>();
    REQUIRE(tape->imageAsset() != nullptr);
    REQUIRE(tape->imageAsset()->decodedByteSize == 70903);
    REQUIRE(tape->mesh() != nullptr);
    REQUIRE(tape->mesh()->vertices().size() == 24);
    REQUIRE(tape->mesh()->indices()->size() == 31 * 3); // Expect 31 triangles.
}

TEST_CASE("duplicating a mesh shares the indices", "[mesh]")
{
    auto file = ReadRiveFile("../../test/assets/tape.riv");

    auto instance1 = file->artboardDefault();
    auto instance2 = file->artboardDefault();
    auto instance3 = file->artboardDefault();

    auto node1 = instance1->find("Tape body.png");
    auto node2 = instance2->find("Tape body.png");
    auto node3 = instance3->find("Tape body.png");
    REQUIRE(node1 != nullptr);
    REQUIRE(node2 != nullptr);
    REQUIRE(node3 != nullptr);
    REQUIRE(node1->is<rive::Image>());
    REQUIRE(node2->is<rive::Image>());
    REQUIRE(node3->is<rive::Image>());

    auto tape1 = node1->as<rive::Image>();
    auto tape2 = node2->as<rive::Image>();
    auto tape3 = node3->as<rive::Image>();
    REQUIRE(tape1->imageAsset() != nullptr);
    REQUIRE(tape1->mesh() != nullptr);
    REQUIRE(tape2->imageAsset() != nullptr);
    REQUIRE(tape2->mesh() != nullptr);
    REQUIRE(tape3->imageAsset() != nullptr);
    REQUIRE(tape3->mesh() != nullptr);

    REQUIRE(tape1->mesh()->indices()->size() == 31 * 3);
    REQUIRE(tape2->mesh()->indices()->size() == 31 * 3);
    REQUIRE(tape3->mesh()->indices()->size() == 31 * 3);

    // Important part, make sure they're all actually the same reference.
    REQUIRE(tape1->mesh()->indices() == tape2->mesh()->indices());
    REQUIRE(tape2->mesh()->indices() == tape3->mesh()->indices());
}
