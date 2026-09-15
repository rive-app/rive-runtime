#include <rive/file.hpp>
#include <rive/bones/bone.hpp>
#include <rive/bones/root_bone.hpp>
#include <rive/constraints/distance_constraint.hpp>
#include <rive/constraints/ik_constraint.hpp>
#include <rive/node.hpp>
#include <rive/math/vec2d.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"

// An IK constraint rebuilds every bone in its chain from that bone's own
// x/y/rotation. A bone above the tip can carry its own constraint that already
// placed it somewhere else, and rebuilding must not drag it back.
TEST_CASE("IK leaves a distance constrained bone above the tip where it was",
          "[file]")
{
    auto file = ReadRiveFile("assets/ik_over_distance_constraint.riv");
    auto artboard = file->artboard();

    auto main = artboard->find<rive::Node>("main");
    auto root = artboard->find<rive::RootBone>("Root Bone");
    auto tip = artboard->find<rive::Bone>("Bone 1");
    auto distanceTarget = artboard->find<rive::Node>("Distance Target");
    auto ikTarget = artboard->find<rive::Node>("IK Target");
    REQUIRE(main != nullptr);
    REQUIRE(root != nullptr);
    REQUIRE(tip != nullptr);
    REQUIRE(distanceTarget != nullptr);
    REQUIRE(ikTarget != nullptr);

    REQUIRE(root->constraints().size() == 1);
    REQUIRE(root->constraints()[0]->is<rive::DistanceConstraint>());
    REQUIRE(tip->constraints().size() == 1);
    REQUIRE(tip->constraints()[0]->is<rive::IKConstraint>());

    artboard->advance(0.0f);

    // The distance constraint pins the root one unit from its target.
    REQUIRE(rive::Vec2D::distance(root->worldTranslation(),
                                  distanceTarget->worldTranslation()) ==
            Approx(1.0f).margin(0.001f));

    // IK still reaches.
    REQUIRE(rive::Vec2D::distance(tip->tipWorldTranslation(),
                                  ikTarget->worldTranslation()) < 0.5f);

    const rive::Vec2D before = root->worldTranslation();

    main->x(main->x() + 100.0f);
    artboard->advance(0.0f);

    // Still pinned: the root slides along the pin circle, it does not travel
    // the 100 units the group moved.
    REQUIRE(rive::Vec2D::distance(root->worldTranslation(),
                                  distanceTarget->worldTranslation()) ==
            Approx(1.0f).margin(0.001f));
    REQUIRE(rive::Vec2D::distance(root->worldTranslation(), before) < 2.0f);
}
