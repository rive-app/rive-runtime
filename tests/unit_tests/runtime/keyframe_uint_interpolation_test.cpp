#include <rive/animation/keyframe_uint.hpp>
#include <rive/generated/core_registry.hpp>
#include <rive/generated/shapes/paint/color_channels_base.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/text/text.hpp>
#include <catch.hpp>

// Uint keyframes hold by default -- most uint properties are enums, ids, or
// mode flags. The R/G/B/A color channels opt in to interpolation via
// "interpolates": true in their def, which generates
// CoreRegistry::isInterpolatableUint. These mirror
// packages/rive_core/test/keyframe_uint_interpolation_test.dart; the two
// implementations must agree on rounding or editor preview and runtime
// playback drift.

using namespace rive;

// Builds a pair of keyframes on the same property and applies the tween at
// `currentTime`, returning what landed on the object.
static uint32_t applyPair(Core* object,
                          uint32_t propertyKey,
                          uint32_t fromValue,
                          uint32_t toValue,
                          float currentTime)
{
    KeyFrameUint from;
    from.frame(0);
    from.value(fromValue);
    from.computeSeconds(60);

    KeyFrameUint to;
    to.frame(60);
    to.value(toValue);
    to.computeSeconds(60);

    from.applyInterpolation(object, propertyKey, currentTime, &to, 1.0f);
    return CoreRegistry::getUint(object, propertyKey);
}

TEST_CASE("the interpolatable uint whitelist covers the color channels",
          "[animation]")
{
    CHECK(CoreRegistry::isInterpolatableUint(
        ColorChannelsBase::colorRedPropertyKey));
    CHECK(CoreRegistry::isInterpolatableUint(
        ColorChannelsBase::colorGreenPropertyKey));
    CHECK(CoreRegistry::isInterpolatableUint(
        ColorChannelsBase::colorBluePropertyKey));
    CHECK(CoreRegistry::isInterpolatableUint(
        ColorChannelsBase::colorAlphaPropertyKey));

    // A uint that animates but must keep holding: an enum in disguise.
    CHECK(!CoreRegistry::isInterpolatableUint(
        TextBase::verticalTrimTopValuePropertyKey));
}

TEST_CASE("color channel keyframes interpolate", "[animation]")
{
    SolidColor solid;
    solid.colorValue((int)0xFF000000);

    CHECK(applyPair(&solid,
                    ColorChannelsBase::colorRedPropertyKey,
                    0u,
                    100u,
                    0.5f) == 50u);

    // Only the keyed channel moved.
    CHECK(solid.colorGreen() == 0u);
    CHECK(solid.colorBlue() == 0u);
    CHECK(solid.colorAlpha() == 0xFFu);
}

TEST_CASE("interpolated channel values round to the nearest byte",
          "[animation]")
{
    SolidColor solid;
    solid.colorValue((int)0xFF000000);

    // 3 * 0.5 = 1.5 rounds to 2, matching the Dart side.
    CHECK(applyPair(&solid,
                    ColorChannelsBase::colorRedPropertyKey,
                    0u,
                    3u,
                    0.5f) == 2u);
}

TEST_CASE("non-whitelisted uints still hold", "[animation]")
{
    // A bitmask passthrough with no direct accessor at runtime; drive it
    // through the registry the way a keyframe would.
    Text text;
    CoreRegistry::setUint(&text, TextBase::verticalTrimTopValuePropertyKey, 0u);

    CHECK(applyPair(&text,
                    TextBase::verticalTrimTopValuePropertyKey,
                    0u,
                    2u,
                    0.5f) == 0u);
}
