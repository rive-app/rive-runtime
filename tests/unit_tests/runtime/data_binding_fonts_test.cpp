#include <rive/file.hpp>
#include <rive/node.hpp>
#include <utils/no_op_renderer.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_font.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/text/font_hb.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("Data bind font", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/data_bind_font_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    auto renderer = silver.makeRenderer();
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();

    // Load the kablammo ttf through the file's factory and set it as the value
    // of the bound ViewModelInstanceAssetFont. This should reshape the text
    // with the new font on the next advance/draw.
    auto fontBytes = ReadFile("assets/kablammo.ttf");
    auto font = silver.decodeFont(fontBytes);
    REQUIRE(font != nullptr);

    auto fontProperty = vmi->propertyValue("fontProperty");
    REQUIRE(fontProperty != nullptr);
    REQUIRE(fontProperty->is<rive::ViewModelInstanceAssetFont>());
    fontProperty->as<rive::ViewModelInstanceAssetFont>()->value(font.get());

    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Click in a square that fires a listener
    // that sets the font to another font value.
    stateMachine->pointerDown(rive::Vec2D(490, 490));
    stateMachine->pointerUp(rive::Vec2D(490, 490));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Click in a square that fires a listener
    // that sets the font to another font property.
    stateMachine->pointerDown(rive::Vec2D(490, 20));
    stateMachine->pointerUp(rive::Vec2D(490, 20));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    CHECK(silver.matches("data_bind_font_test"));
}

// Deterministic (no golden) coverage of the ViewModelInstanceAssetFont value
// API: assigning a decoded font stores it on the property's backing FontAsset,
// and passing null clears it.
TEST_CASE("Font data bind stores and clears the font on the property",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/data_bind_font_test.riv");

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);

    auto property = vmi->propertyValue("fontProperty");
    REQUIRE(property != nullptr);
    REQUIRE(property->is<rive::ViewModelInstanceAssetFont>());
    auto fontProperty = property->as<rive::ViewModelInstanceAssetFont>();
    REQUIRE(fontProperty->asset() != nullptr);

    // Assigning a decoded font stores it on the property's backing FontAsset.
    auto fontBytes = ReadFile("assets/kablammo.ttf");
    auto font = HBFont::Decode(fontBytes);
    REQUIRE(font != nullptr);
    fontProperty->value(font.get());
    stateMachine->advanceAndApply(0.0f);
    CHECK(fontProperty->asset()->font().get() == font.get());

    // Swapping to a different font updates the backing FontAsset.
    auto font2Bytes = ReadFile("assets/nabla.ttf");
    auto font2 = HBFont::Decode(font2Bytes);
    REQUIRE(font2 != nullptr);
    fontProperty->value(font2.get());
    stateMachine->advanceAndApply(0.0f);
    CHECK(fontProperty->asset()->font().get() == font2.get());

    // Passing null clears the backing font.
    fontProperty->value(nullptr);
    stateMachine->advanceAndApply(0.0f);
    CHECK(fontProperty->asset()->font() == nullptr);
}