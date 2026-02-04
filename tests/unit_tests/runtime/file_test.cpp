#include "rive/file.hpp"
#include "rive/node.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/mesh.hpp"
#include "utils/no_op_renderer.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

TEST_CASE("transform order is as expected", "[transform]")
{
    auto translation = rive::Mat2D::fromTranslate(10.0f, 20.0f);
    auto rotation = rive::Mat2D::fromRotation(3.14f / 2.0f);
    auto scale = rive::Mat2D::fromScale(2.0f, 3.0f);

    auto xform = translation * rotation * scale;
    auto xform2 = rive::Mat2D::fromRotation(3.14f / 2.0f);
    xform2[0] *= 2.0f;
    xform2[1] *= 2.0f;
    xform2[2] *= 3.0f;
    xform2[3] *= 3.0f;
    xform2[4] = 10.0f;
    xform2[5] = 20.0f;

    REQUIRE(xform2 == xform);
}

TEST_CASE("file can be read", "[file]")
{
    auto file = ReadRiveFile("assets/two_artboards.riv");

    // Default artboard should be named Two.
    REQUIRE(file->artboard()->name() == "Two");

    // There should be a second artboard named One.
    REQUIRE(file->artboard("One") != nullptr);
}

TEST_CASE("file with bad blend mode fails to load", "[file]")
{
    std::vector<uint8_t> bytes = ReadFile("assets/solar-system.riv");

    rive::ImportResult result;
    auto file = rive::File::import(bytes, &gNoOpFactory, &result, nullptr);
    CHECK(result == rive::ImportResult::malformed);
}

TEST_CASE("file with animation can be read", "[file]")
{
    auto file = ReadRiveFile("assets/juice.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard->name() == "New Artboard");

    auto shin = artboard->find("shin_right");
    REQUIRE(shin != nullptr);
    REQUIRE(shin->is<rive::Node>());

    auto shinNode = shin->as<rive::Node>();
    REQUIRE(shinNode->parent() != nullptr);
    REQUIRE(shinNode->parent()->name() == "leg_right");
    REQUIRE(shinNode->parent()->parent() != nullptr);
    REQUIRE(shinNode->parent()->parent()->name() == "root");
    REQUIRE(shinNode->parent()->parent() != nullptr);
    REQUIRE(shinNode->parent()->parent()->parent() != nullptr);
    REQUIRE(shinNode->parent()->parent()->parent() == artboard);

    auto walkAnimation = artboard->animation("walk");
    REQUIRE(walkAnimation != nullptr);
    REQUIRE(walkAnimation->numKeyedObjects() == 22);
}

TEST_CASE("artboards can be counted and accessed via index or name", "[file]")
{
    auto file = ReadRiveFile("assets/dependency_test.riv");

    // The artboards caqn be counted
    REQUIRE(file->artboardCount() == 1);

    // Artboards can be access by index
    REQUIRE(file->artboard(0) != nullptr);

    // Artboards can be accessed by name
    REQUIRE(file->artboard("Blue") != nullptr);
}

TEST_CASE("dependencies are as expected", "[file]")
{
    // ┌────┐
    // │Blue│
    // └────┘
    //    │ ┌───┐
    //    └▶│ A │
    //      └───┘
    //        │ ┌───┐
    //        └▶│ B │
    //          └───┘
    //            │ ┌───┐
    //            ├▶│ C │
    //            │ └───┘
    //            │ ┌─────────┐
    //            └▶│Rectangle│
    //              └─────────┘
    //                   │ ┌──────────────┐
    //                   └▶│Rectangle Path│
    //                     └──────────────┘
    auto file = ReadRiveFile("assets/dependency_test.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard->name() == "Blue");

    auto nodeA = artboard->find<rive::Node>("A");
    auto nodeB = artboard->find<rive::Node>("B");
    auto nodeC = artboard->find<rive::Node>("C");
    auto shape = artboard->find<rive::Shape>("Rectangle");
    auto path = artboard->find<rive::Path>("Rectangle Path");
    REQUIRE(nodeA != nullptr);
    REQUIRE(nodeB != nullptr);
    REQUIRE(nodeC != nullptr);
    REQUIRE(shape != nullptr);
    REQUIRE(path != nullptr);

    REQUIRE(nodeA->parent() == artboard);
    REQUIRE(nodeB->parent() == nodeA);
    REQUIRE(nodeC->parent() == nodeB);
    REQUIRE(shape->parent() == nodeB);
    REQUIRE(path->parent() == shape);

    REQUIRE(nodeB->dependents().size() == 2);

    REQUIRE(artboard->graphOrder() == 0);
    REQUIRE(nodeA->graphOrder() > artboard->graphOrder());
    REQUIRE(nodeB->graphOrder() > nodeA->graphOrder());
    REQUIRE(nodeC->graphOrder() > nodeB->graphOrder());
    REQUIRE(shape->graphOrder() > nodeB->graphOrder());
    REQUIRE(path->graphOrder() > shape->graphOrder());

    artboard->advance(0.0f);

    auto world = shape->worldTransform();
    REQUIRE(world[4] == 39.203125f);
    REQUIRE(world[5] == 29.535156f);
}

TEST_CASE("long name in object is parsed correctly", "[file]")
{
    auto file = ReadRiveFile("assets/long_name.riv");
    auto artboard = file->artboard();

    // Expect all object in file to be loaded, in this case 7
    REQUIRE(artboard->objects().size() == 7);
}

TEST_CASE("file with in-band images can have the stripped", "[file]")
{
    FILE* fp = fopen("assets/jellyfish_test.riv", "rb");
    REQUIRE(fp != nullptr);

    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    REQUIRE(fread(bytes.data(), 1, length, fp) == length);
    fclose(fp);

    rive::ImportResult result;
    auto file = rive::File::import(bytes, &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file.get() != nullptr);
    REQUIRE(file->artboard() != nullptr);

    // Default artboard should be named Two.
    REQUIRE(file->artboard()->name() == "Jellyfish");

    // Strip nothing should result in the same file.
    {
        rive::ImportResult stripResult;
        auto strippedBytes = rive::File::stripAssets(bytes, {}, &stripResult);
        REQUIRE(stripResult == rive::ImportResult::success);
        REQUIRE(bytes.size() == strippedBytes.size());
        REQUIRE(std::memcmp(bytes.data(), strippedBytes.data(), bytes.size()) ==
                0);
    }

    // Strip image assets should result in a smaller file.
    {
        rive::ImportResult stripResult;
        auto strippedBytes =
            rive::File::stripAssets(bytes,
                                    {rive::ImageAsset::typeKey},
                                    &stripResult);
        REQUIRE(stripResult == rive::ImportResult::success);
        REQUIRE(strippedBytes.size() < bytes.size());
    }
}

TEST_CASE("file a bad skin (no parent skinnable) doesn't crash", "[file]")
{
    FILE* fp = fopen("assets/bad_skin.riv", "rb");
    REQUIRE(fp != nullptr);

    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    REQUIRE(fread(bytes.data(), 1, length, fp) == length);
    fclose(fp);

    rive::ImportResult result;
    auto file = rive::File::import(bytes, &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file.get() != nullptr);
    REQUIRE(file->artboard() != nullptr);

    REQUIRE(file->artboard()->name() == "Illustration WOman.svg");
    auto artboard = file->artboardDefault();
    artboard->updateComponents();
    auto paths = artboard->find<rive::PointsPath>();
    for (auto path : paths)
    {
        path->markPathDirty();
    }
    artboard->updateComponents();
}

TEST_CASE("file with bad keyed property loads", "[file]")
{
    FILE* fp = fopen("assets/magic_alley_db_reduced_export.riv", "rb");
    REQUIRE(fp != nullptr);

    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    REQUIRE(fread(bytes.data(), 1, length, fp) == length);
    fclose(fp);

    rive::ImportResult result;
    auto file = rive::File::import(bytes, &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file.get() != nullptr);
    REQUIRE(file->artboard() != nullptr);

    REQUIRE(file->artboard()->name() == "Artboard");
    auto artboard = file->artboardDefault();
    artboard->updateComponents();
}

TEST_CASE("file can be read with verified signed scripts", "[file]")
{
    auto file = ReadRiveFile("assets/joel_signed.riv");

    for (auto asset : file->assets())
    {
        if (asset->is<rive::ScriptAsset>())
        {
            // All script assets should've been verified by the time the file is
            // loaded.
            CHECK(asset->as<rive::ScriptAsset>()->verified());
        }
    }
}

TEST_CASE(
    "Test deterministic mode for randomization and elastic scroll physics",
    "[file]")
{
    rive::SerializingFactory silver;
    rive::File::deterministicMode = true;
    auto file = ReadRiveFile("assets/deterministic_mode.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    stateMachine->pointerDown(rive::Vec2D(artboard->width() / 2.0f, 400.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(0.25f / 0.016f);
    float yPos = 400.0f;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(artboard->width() / 2.0f, yPos),
                                  0.016f);
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
        yPos -= 40.0f;
    }
    silver.addFrame();
    stateMachine->pointerMove(rive::Vec2D(artboard->width() / 2.0f, yPos),
                              0.016f);
    stateMachine->pointerUp(rive::Vec2D(artboard->width() / 2.0f, yPos));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("deterministic_mode"));
    rive::File::deterministicMode = false;
}
