#include <rive/core/field_types/core_uint_type.hpp>
#include <rive/generated/core_registry.hpp>
#include <rive/generated/shapes/paint/color_channels_base.hpp>
#include <rive/node.hpp>
#include <rive/shapes/paint/gradient_stop.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <utils/serializing_factory.hpp>
#include <rive_file_reader.hpp>
#include <catch.hpp>
#include <rive/animation/state_machine_instance.hpp>

// The per-channel color properties (colorRed/Green/Blue/Alpha) are shared by
// SolidColor and GradientStop via the ColorChannels mixin. They are passthrough
// views into the packed ARGB colorValue: they store nothing of their own and
// both components share the same property keys.

using namespace rive;

TEST_CASE("color channels read the right byte of colorValue", "[color]")
{
    SolidColor solid;
    solid.colorValue((int)0xAABBCCDD);
    CHECK(solid.colorAlpha() == 0xAAu);
    CHECK(solid.colorRed() == 0xBBu);
    CHECK(solid.colorGreen() == 0xCCu);
    CHECK(solid.colorBlue() == 0xDDu);

    GradientStop stop;
    stop.colorValue((int)0x11223344);
    CHECK(stop.colorAlpha() == 0x11u);
    CHECK(stop.colorRed() == 0x22u);
    CHECK(stop.colorGreen() == 0x33u);
    CHECK(stop.colorBlue() == 0x44u);
}

TEST_CASE("setting a channel writes only its byte and recomposes colorValue",
          "[color]")
{
    SolidColor solid;
    solid.colorValue((int)0xAABBCCDD);

    solid.colorGreen(0x11u);
    CHECK((uint32_t)solid.colorValue() == 0xAABB11DDu);
    // Other channels untouched.
    CHECK(solid.colorAlpha() == 0xAAu);
    CHECK(solid.colorRed() == 0xBBu);
    CHECK(solid.colorBlue() == 0xDDu);

    solid.colorAlpha(0x00u);
    CHECK((uint32_t)solid.colorValue() == 0x00BB11DDu);
}

TEST_CASE("channels clamp to 255 instead of wrapping", "[color]")
{
    SolidColor solid;
    solid.colorValue((int)0x00000000);

    // A value wider than the 8-bit field saturates at 255 rather than
    // overflowing into the neighbouring byte (300 & 0xFF would wrap to 44).
    solid.colorRed(300u);
    CHECK(solid.colorRed() == 0xFFu);
    CHECK((uint32_t)solid.colorValue() == 0x00FF0000u);

    // Same guarantee through the registry (data binding / animation path).
    CoreRegistry::setUint(&solid,
                          ColorChannelsBase::colorAlphaPropertyKey,
                          1000u);
    CHECK(CoreRegistry::getUint(&solid,
                                ColorChannelsBase::colorAlphaPropertyKey) ==
          0xFFu);
    CHECK((uint32_t)solid.colorValue() == 0xFFFF0000u);
}

TEST_CASE("ColorChannelsBase::from resolves consumers, null otherwise",
          "[color]")
{
    SolidColor solid;
    GradientStop stop;
    Node node;

    CHECK(ColorChannelsBase::from(&solid) != nullptr);
    CHECK(ColorChannelsBase::from(&stop) != nullptr);
    // A Core object that does not include the mixin resolves to null.
    CHECK(ColorChannelsBase::from(&node) == nullptr);

    // The resolved interface reads/writes the host mask.
    solid.colorValue((int)0xFF000000);
    ColorChannelsBase::from(&solid)->colorRed(0x80u);
    CHECK((uint32_t)solid.colorValue() == 0xFF800000u);
}

TEST_CASE("shared channel keys dispatch through CoreRegistry for both types",
          "[color]")
{
    SolidColor solid;
    GradientStop stop;

    // The SAME property keys route to both concrete types.
    CoreRegistry::setUint(&solid,
                          ColorChannelsBase::colorRedPropertyKey,
                          0x34u);
    CoreRegistry::setUint(&stop, ColorChannelsBase::colorRedPropertyKey, 0x34u);
    CHECK(
        CoreRegistry::getUint(&solid, ColorChannelsBase::colorRedPropertyKey) ==
        0x34u);
    CHECK(
        CoreRegistry::getUint(&stop, ColorChannelsBase::colorRedPropertyKey) ==
        0x34u);
    CHECK(solid.colorRed() == 0x34u);
    CHECK(stop.colorRed() == 0x34u);

    // A channel write is a masked read-modify-write of colorValue.
    solid.colorValue((int)0x00000000);
    CoreRegistry::setUint(&solid,
                          ColorChannelsBase::colorAlphaPropertyKey,
                          0xCDu);
    CHECK((uint32_t)solid.colorValue() == 0xCD000000u);
    CHECK(CoreRegistry::getUint(&solid,
                                ColorChannelsBase::colorAlphaPropertyKey) ==
          0xCDu);
}

TEST_CASE("objectSupportsProperty is true for channels on consumers only",
          "[color]")
{
    SolidColor solid;
    GradientStop stop;
    Node node;

    CHECK(CoreRegistry::objectSupportsProperty(
        &solid,
        ColorChannelsBase::colorRedPropertyKey));
    CHECK(CoreRegistry::objectSupportsProperty(
        &stop,
        ColorChannelsBase::colorAlphaPropertyKey));
    CHECK_FALSE(CoreRegistry::objectSupportsProperty(
        &node,
        ColorChannelsBase::colorRedPropertyKey));
}

TEST_CASE("channel keys report a uint field type for data binding", "[color]")
{
    // propertyFieldId must resolve the channels (they are uint-valued) so data
    // binding can build the right target value instead of crashing on -1.
    CHECK(CoreRegistry::propertyFieldId(
              ColorChannelsBase::colorRedPropertyKey) == +CoreUintType::id);
    CHECK(CoreRegistry::propertyFieldId(
              ColorChannelsBase::colorAlphaPropertyKey) == +CoreUintType::id);
}

TEST_CASE("Silver test of passthrough properties", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/color_passthrough_test.riv", &silver);
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(3.0f / 0.25f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.25f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("color_passthrough_test"));
}
