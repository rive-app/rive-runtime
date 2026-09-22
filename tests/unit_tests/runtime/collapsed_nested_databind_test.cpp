#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

using namespace rive;

// assets/collapsed_nested_databind.riv
//
//   host ........ Solo "gate", activeComponentId bound to `soloIndex`
//                   [0] "off" -- empty, active by default
//                   [1] "on"  -- mounts bodyB, so bodyB starts COLLAPSED
//   bodyB ....... node "neck" at y=-70, computedWorldY -> `neckY` (toSource)
//                 mounts deep
//   deep ........ node "mark" at y=-42, computedWorldY -> `deepY` (toSource)
//                 node "follower", x <- `feed`, computedWorldX -> `echo`
//
// The view model's authored defaults are deliberately values no bind would
// ever produce -- neckY=1234, deepY=4321, echo=777 -- because the failure
// being guarded against writes 0, which is indistinguishable from an
// unset number.
//
// A collapsed mount never advances (NestedArtboard::advanceComponent bails on
// isCollapsed), so its world transforms sit at identity. Collapse also stops
// at the artboard boundary -- NestedArtboard::collapse is semantic-only -- so
// nothing inside the mount is flagged and DataBind::canSkip cannot see that
// it is hidden. Without the guard in NestedArtboard::updateDataBinds, every
// toSource bind in there publishes an uncomputed 0 into the shared view model
// and anything reading the property, in any artboard, picks it up.
//
// `deep` is the part that matters most: it is one level below the collapsed
// mount and never sees a collapse transition of its own, so suppressing only
// the mount's own binds leaves it publishing.
TEST_CASE("a collapsed nested artboard does not write to its source",
          "[databind]")
{
    auto file = ReadRiveFile("assets/collapsed_nested_databind.riv");
    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);

    auto number = [&](const char* name) {
        return vmi->propertyValue(name)
            ->as<ViewModelInstanceNumber>()
            ->propertyValue();
    };
    auto setNumber = [&](const char* name, float value) {
        vmi->propertyValue(name)->as<ViewModelInstanceNumber>()->propertyValue(
            value);
    };

    setNumber("feed", 55.0f);

    // Collapsed: the authored defaults have to survive. A regression writes 0
    // to all three.
    for (int i = 0; i < 10; i++)
    {
        stateMachine->advanceAndApply(0.016f);
    }
    CHECK(number("neckY") == Approx(1234.0f));
    CHECK(number("deepY") == Approx(4321.0f));
    CHECK(number("echo") == Approx(777.0f));

    // Uncollapse. Everything that was skipped has to catch up on its own --
    // dirt is queued by the source notifying its dependents, independent of
    // whether the container was being drained, so the first pass after the
    // Solo switches applies whatever the sources last moved to. No explicit
    // replay is involved; if one is ever added, this still has to hold.
    setNumber("soloIndex", 1.0f);
    for (int i = 0; i < 10; i++)
    {
        stateMachine->advanceAndApply(0.016f);
    }
    CHECK(number("neckY") == Approx(-70.0f));
    CHECK(number("deepY") == Approx(-42.0f));
    // follower.x took `feed` source->target while hidden or on the way back,
    // and its world x published out again.
    CHECK(number("echo") == Approx(55.0f));

    // Collapse again: the mount stops publishing, so the last values it wrote
    // stay put rather than decaying to 0.
    setNumber("soloIndex", 0.0f);
    for (int i = 0; i < 10; i++)
    {
        stateMachine->advanceAndApply(0.016f);
    }
    CHECK(number("neckY") == Approx(-70.0f));
    CHECK(number("deepY") == Approx(-42.0f));
    CHECK(number("echo") == Approx(55.0f));
}
