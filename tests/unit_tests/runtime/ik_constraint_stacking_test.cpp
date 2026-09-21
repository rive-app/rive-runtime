#include <rive/file.hpp>
#include <rive/bones/bone.hpp>
#include <rive/bones/root_bone.hpp>
#include <rive/constraints/ik_constraint.hpp>
#include <rive/constraints/rotation_constraint.hpp>
#include <rive/node.hpp>
#include <rive/math/mat2d.hpp>
#include <rive/math/vec2d.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"

// An IK constraint blends against the pose it finds on the chain, so a zero
// strength one has to leave that pose alone.

TEST_CASE("a zero strength IK does not undo the IK stacked before it", "[file]")
{
    auto file = ReadRiveFile("assets/ik_stacked_constraints.riv");
    auto artboard = file->artboard();

    auto tip = artboard->find<rive::Bone>("Bone 1");
    auto ikTarget = artboard->find<rive::Node>("IK Target");
    REQUIRE(tip != nullptr);
    REQUIRE(ikTarget != nullptr);

    // The one that reaches runs first; the idle one must not undo it.
    REQUIRE(tip->constraints().size() == 2);
    auto solve = tip->constraints()[0];
    auto off = tip->constraints()[1];
    REQUIRE(solve->is<rive::IKConstraint>());
    REQUIRE(off->is<rive::IKConstraint>());
    REQUIRE(solve->strength() == Approx(1.0f));
    REQUIRE(off->strength() == Approx(0.0f));

    artboard->advance(0.0f);

    REQUIRE(rive::Vec2D::distance(tip->tipWorldTranslation(),
                                  ikTarget->worldTranslation()) < 0.5f);

    // Still follows when the target moves, the way dragging a handle does.
    ikTarget->x(300.0f);
    ikTarget->y(160.0f);
    artboard->advance(0.0f);

    REQUIRE(rive::Vec2D::distance(tip->tipWorldTranslation(),
                                  ikTarget->worldTranslation()) < 0.5f);
}

TEST_CASE("a zero strength IK leaves a rotation constrained chain bone alone",
          "[file]")
{
    auto file = ReadRiveFile("assets/ik_over_rotation_constraint.riv");
    auto artboard = file->artboard();

    auto root = artboard->find<rive::RootBone>("Root Bone");
    auto tip = artboard->find<rive::Bone>("Bone 1");
    REQUIRE(root != nullptr);
    REQUIRE(tip != nullptr);

    // The IK reaches back over the root, so both write the root's angle.
    REQUIRE(root->constraints().size() == 1);
    REQUIRE(root->constraints()[0]->is<rive::RotationConstraint>());
    REQUIRE(tip->constraints().size() == 1);
    REQUIRE(tip->constraints()[0]->is<rive::IKConstraint>());
    REQUIRE(tip->constraints()[0]->strength() == Approx(0.0f));

    artboard->advance(0.0f);

    // Authored at 0, constrained to 45. Reading 0 means the IK rebuilt it.
    REQUIRE(root->worldTransform().decompose().rotation() ==
            Approx(0.7853982f).margin(0.001f));
}
