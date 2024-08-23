#include <rive/file.hpp>
#include <rive/constraints/ik_constraint.hpp>
#include <rive/node.hpp>
#include <rive/math/vec2d.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/bones/skin.hpp>
#include <rive/bones/bone.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("ik with skinned bones orders correctly", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/complex_ik_dependency.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::Bone>("One") != nullptr);
    auto one = artboard->find<rive::Bone>("One");

    REQUIRE(artboard->find<rive::Bone>("Two") != nullptr);
    auto two = artboard->find<rive::Bone>("Two");
    rive::Skin* skin = nullptr;
    for (auto object : artboard->objects())
    {
        if (object->is<rive::Skin>())
        {
            skin = object->as<rive::Skin>();
            break;
        }
    }

    REQUIRE(skin != nullptr);
    REQUIRE(two->constraints()[0]->is<rive::IKConstraint>());

    REQUIRE(skin->graphOrder() > one->graphOrder());
    REQUIRE(skin->graphOrder() > two->graphOrder());
}
