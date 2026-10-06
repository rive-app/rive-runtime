#include <rive/artboard.hpp>
#include <rive/constraints/rotation_constraint.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/bones/bone.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/math/transform_components.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("rotation constraint updates world transform", "[file]")
{
    auto file = ReadRiveFile("assets/rotation_constraint.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::TransformComponent>("target") != nullptr);
    auto target = artboard->find<rive::TransformComponent>("target");

    REQUIRE(artboard->find<rive::TransformComponent>("rect") != nullptr);
    auto rectangle = artboard->find<rive::TransformComponent>("rect");

    artboard->advance(0.0f);
    auto targetComponents = target->worldTransform().decompose();
    auto rectComponents = rectangle->worldTransform().decompose();

    REQUIRE(targetComponents.rotation() == rectComponents.rotation());
}

static float worldRotation(rive::Artboard* artboard, const char* name)
{
    auto component = artboard->find<rive::TransformComponent>(name);
    REQUIRE(component != nullptr);
    return component->worldTransform().decompose().rotation();
}

TEST_CASE("rotation constraint limits hold across the half turn", "[file]")
{
    auto file = ReadRiveFile("assets/rotation_constraint_wrap.riv");
    auto artboard = file->artboard();
    artboard->advance(0.0f);

    // A min of -180 degrees used to never clamp since decomposed angles are
    // never below it.
    CHECK(std::abs(worldRotation(artboard, "min_at_half_turn")) ==
          Approx(rive::math::PI).margin(0.0001f));
    // 200 degrees is inside 150 to 210 even though it decomposes to -160.
    CHECK(worldRotation(artboard, "range_across_half_turn") ==
          Approx(-2.7925268f).margin(0.0001f));
    CHECK(worldRotation(artboard, "max_zero_clamped") ==
          Approx(0.0f).margin(0.0001f));
    CHECK(worldRotation(artboard, "max_zero_free") ==
          Approx(-2.5f).margin(0.0001f));
}

namespace
{
struct LimitCase
{
    float rotation;
    bool min;
    float minValue;
    bool max;
    float maxValue;
    float expected;
};

float degrees(float value) { return value * rive::math::PI / 180.0f; }

// A node under the artboard with an untargeted rotation constraint, so the
// limits apply to its own world rotation.
float constrainedRotation(const LimitCase& limits)
{
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);
    artboard.addObject(&artboard);
    auto node = new rive::Node();
    node->rotation(degrees(limits.rotation));
    artboard.addObject(node);
    node->parentId(artboard.idOf(&artboard));
    auto constraint = new rive::RotationConstraint();
    constraint->min(limits.min);
    constraint->minValue(degrees(limits.minValue));
    constraint->max(limits.max);
    constraint->maxValue(degrees(limits.maxValue));
    artboard.addObject(constraint);
    constraint->parentId(artboard.idOf(node));
    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.advance(0.0f);
    return node->worldTransform().decompose().rotation();
}
} // namespace

TEST_CASE("rotation constraint limits clamp off center angles", "[constraint]")
{
    // Mirrored in rive_core's rotation_constraint_test.dart.
    const LimitCase cases[] = {
        {-120, false, 0, true, 90, 90},
        {120, true, -90, false, 0, -90},
        {-150, true, 0, true, 90, 90},
        {-60, true, 0, true, 90, 0},
        {45, true, 90, true, 0, 90},
        {-170, true, 90, true, 0, 90},
        {170, true, -180, false, 0, -180},
        {200, true, 150, true, 210, -160},
    };
    for (const auto& limits : cases)
    {
        float rotation = constrainedRotation(limits);
        float expected = degrees(limits.expected);
        // The half turn decomposes to either sign.
        if (std::abs(expected) == rive::math::PI)
        {
            rotation = std::abs(rotation);
            expected = std::abs(expected);
        }
        CAPTURE(limits.rotation, limits.minValue, limits.maxValue);
        CHECK(rotation == Approx(expected).margin(0.0001f));
    }
}
