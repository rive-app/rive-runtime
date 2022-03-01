#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/image.hpp>
#include <rive/shapes/mesh.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/relative_local_asset_resolver.hpp>
#include "no_op_renderer.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("image with mesh loads correctly", "[assets]") {
    RiveFileReader reader("../../test/assets/tape.riv");
    auto file = reader.file();

    auto node = file->artboard()->find("Tape body.png");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Image>());
    auto tape = node->as<rive::Image>();
    REQUIRE(tape->imageAsset() != nullptr);
    REQUIRE(tape->imageAsset()->decodedByteSize == 70903);
    REQUIRE(tape->mesh() != nullptr);
    REQUIRE(tape->mesh()->vertices().size() == 24);
}
