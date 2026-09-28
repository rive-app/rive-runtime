#include "rive/artboard.hpp"
#include "rive/bones/skin.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/constraints/targeted_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// zombie_skins.riv: 25 skins and 17 constraints (IK, translation, rotation and
// scale) in its default artboard. Advanced without a state machine, nothing
// moves once the first update has settled it.

TEST_CASE("a fade neither re-skins nor re-constrains", "[opacity]")
{
    auto file = ReadRiveFile("assets/zombie_skins.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);
    auto skins = artboard->find<rive::Skin>();
    auto constraints = artboard->find<rive::Constraint>();
    REQUIRE(!skins.empty());
    REQUIRE(!constraints.empty());

    // The new opacity reaches every skin through its bones and every
    // constraint through the component it constrains. Neither moves anything.
    artboard->opacity(0.5f);
    for (auto* skin : skins)
    {
        // The skinned path (or mesh), which a re-skin marks dirty.
        auto* skinned = skin->parent();
        CHECK_FALSE(skinned->hasDirt(rive::ComponentDirt::Path));
        CHECK_FALSE(skinned->hasDirt(rive::ComponentDirt::Vertices));
    }
    for (auto* constraint : constraints)
    {
        CHECK_FALSE(
            constraint->parent()->hasDirt(rive::ComponentDirt::Transform));
    }
}

// A constraint collapses along with the component it constrains (a Solo hides
// both), and neither updates while hidden. Its target can still move in the
// meantime, alongside a fade: once shown again, the constrained part has to
// land where it would have without ever being hidden.
TEST_CASE("constrained parts hidden during a fade come back constrained",
          "[opacity]")
{
    auto file = ReadRiveFile("assets/zombie_skins.riv");
    auto hidden = file->artboardDefault();
    auto shown = file->artboardDefault();
    hidden->advance(0.0f);
    shown->advance(0.0f);
    auto hiddenConstraints = hidden->find<rive::TargetedConstraint>();
    auto shownConstraints = shown->find<rive::TargetedConstraint>();
    REQUIRE(!hiddenConstraints.empty());
    REQUIRE(hiddenConstraints.size() == shownConstraints.size());
    float opacity = 1.0f;
    for (size_t i = 0; i < hiddenConstraints.size(); i++)
    {
        auto* constrained =
            hiddenConstraints[i]->parent()->as<rive::TransformComponent>();
        auto* reference =
            shownConstraints[i]->parent()->as<rive::TransformComponent>();
        auto* hiddenObject = hidden->resolve(hiddenConstraints[i]->targetId());
        auto* shownObject = shown->resolve(shownConstraints[i]->targetId());
        if (hiddenObject == nullptr ||
            !hiddenObject->is<rive::TransformComponent>())
        {
            continue;
        }
        auto* hiddenTarget = hiddenObject->as<rive::TransformComponent>();
        auto* shownTarget = shownObject->as<rive::TransformComponent>();
        INFO(constrained->name());
        opacity = opacity == 1.0f ? 0.5f : 1.0f;

        constrained->collapse(true);
        hidden->advance(0.0f);
        hiddenTarget->rotation(hiddenTarget->rotation() + 0.3f);
        hidden->opacity(opacity);
        hidden->advance(0.0f);
        constrained->collapse(false);
        hidden->advance(0.0f);

        shownTarget->rotation(shownTarget->rotation() + 0.3f);
        shown->opacity(opacity);
        shown->advance(0.0f);

        CHECK(constrained->worldTransform() == reference->worldTransform());
    }
}
