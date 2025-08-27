#include "rive/file.hpp"
#include "rive/node.hpp"
#include "rive/animation/elastic_interpolator.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/animation/elastic_ease.hpp"
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include <cstdio>

TEST_CASE("test elastic easing loads properly", "[file]")
{
    auto file = ReadRiveFile("assets/test_elastic.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);

    REQUIRE(artboard->find<rive::ElasticInterpolator>().size() == 1);

    auto interpolator = artboard->find<rive::ElasticInterpolator>()[0];
    REQUIRE(interpolator->easing() == rive::Easing::easeOut);
    REQUIRE(interpolator->amplitude() == 1.0f);
    REQUIRE(interpolator->period() == 0.25f);

    REQUIRE(artboard->find<rive::Shape>().size() == 1);

    auto shape = artboard->find<rive::Shape>()[0];
    REQUIRE(shape->x() == Approx(145.19f));
    auto animation = artboard->animation("Timeline 1");
    REQUIRE(animation != nullptr);
    // Go to frame 15.
    animation->apply(artboard, 7.0f / animation->fps(), 1.0f);
    REQUIRE(shape->x() == Approx(423.98f));

    animation->apply(artboard, 14.0f / animation->fps(), 1.0f);
    REQUIRE(shape->x() == Approx(303.995f));
}

TEST_CASE("elastic easer computes correct actual amplitude", "[animation]")
{
    rive::ElasticEase easer(0.5f, 3.14f);
    REQUIRE(easer.computeActualAmplitude(0.0f) == 1.0f);
    REQUIRE(easer.computeActualAmplitude(1.57f) == 0.5f);
    REQUIRE(easer.easeOut(0.22f) == Catch::Detail::Approx(0.8307f));
    REQUIRE(easer.easeIn(1.58f) == Catch::Detail::Approx(14.01086f));
    REQUIRE(easer.easeInOut(1.58f) == Catch::Detail::Approx(1.0f));
}
