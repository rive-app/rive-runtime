#include <rive/file.hpp>
#include <rive/constraints/ik_constraint.hpp>
#include <rive/node.hpp>
#include <rive/math/vec2d.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/bones/skin.hpp>
#include <rive/bones/bone.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include <cstdio>

TEST_CASE("ik with skinned bones orders correctly", "[file]")
{
    auto file = ReadRiveFile("assets/complex_ik_dependency.riv");

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

TEST_CASE("IK constraint with non full strength", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/ik_anim_test.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    auto renderer = silver.makeRenderer();

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    int frames = (int)(2.0f / 0.5f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.5f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("ik_anim_test"));
}
