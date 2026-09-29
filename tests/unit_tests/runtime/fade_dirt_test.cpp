#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/bones/skin.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/constraints/targeted_constraint.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/trim_path.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/shape_paint_path.hpp"
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

static bool hasSkinnedPath(rive::Shape* shape)
{
    for (auto path : shape->paths())
    {
        if (path->is<rive::PointsPath>() &&
            path->as<rive::PointsPath>()->skin() != nullptr)
        {
            return true;
        }
    }
    return false;
}

TEST_CASE("a skinned shape measures its trim path when it fades in",
          "[opacity]")
{
    // electrified_button_simple hides skinned shapes, stroked with an animated
    // trim path, at zero opacity and fades them in on hover. A paint skips its
    // effects while it is transparent and relies on being re-invalidated when
    // it shows. A transparent shape normally defers its path, and rebuilding
    // it on show is what re-invalidates the paint. Skinned shapes never defer,
    // and once a fade stopped re-skinning them nothing re-invalidated their
    // paints, so they faded in with an empty trim path.
    auto file = ReadRiveFile("assets/electrified_button_simple.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineNamed("button");
    REQUIRE(machine != nullptr);
    machine->advanceAndApply(0.0f);

    std::vector<std::pair<rive::TrimPath*, rive::Stroke*>> hidden;
    for (auto trim : artboard->find<rive::TrimPath>())
    {
        auto parent = trim->parent();
        if (parent == nullptr || !parent->is<rive::Stroke>())
        {
            continue;
        }
        auto stroke = parent->as<rive::Stroke>();
        auto shape = stroke->parent();
        if (shape != nullptr && shape->is<rive::Shape>() &&
            hasSkinnedPath(shape->as<rive::Shape>()) &&
            stroke->renderOpacity() == 0)
        {
            hidden.emplace_back(trim, stroke);
        }
    }
    REQUIRE(!hidden.empty());

    machine->pointerMove(rive::Vec2D(250.0f, 250.0f));
    machine->advanceAndApply(0.0f);

    for (auto [trim, stroke] : hidden)
    {
        REQUIRE(stroke->renderOpacity() != 0);
        auto effectPath = trim->effectPath(stroke);
        REQUIRE(effectPath != nullptr);
        CHECK(!effectPath->rawPath()->empty());
    }
}
