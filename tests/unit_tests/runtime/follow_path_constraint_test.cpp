#include "rive/animation/state_machine_instance.hpp"
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/math/transform_components.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
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

TEST_CASE("Animate shape along follow path", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/follow_path_shapes.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 60;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("follow_path_animate_shape"));
}

TEST_CASE("Animate solo along follow path", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/follow_path_solos.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 240;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("follow_path_animate_solo"));
}

TEST_CASE("Animate follow path target path", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/follow_path_path.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 120;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("follow_path_animate_target"));
}

TEST_CASE("Text follow path modifier", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/text_follow_path_shape_length.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 10;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("text_follow_path_shape_length"));
}