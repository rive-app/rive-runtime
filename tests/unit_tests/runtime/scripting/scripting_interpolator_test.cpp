#include "catch.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout_component.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/node.hpp"
#include "rive/scripted/scripted_interpolator.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

// Both fixtures run one interpolator script, curve.luau, built from RML with
// the rive CLI. It is linear until its init() runs and an ease-in quad (v*v)
// after, so a curve that eases proves the consumer ran a live, initialized
// instance rather than the shared template or an uninitialized copy:
//
//   type Curve = { power: number }
//   function init(self: Curve, context: Context): boolean
//       self.power = 2
//       return true
//   end
//   function transform(self: Curve, value: number): number
//       return value ^ self.power
//   end
//   -- transformValue likewise; the generator returns { power = 1, ... }.
//
// scripted_interpolator_layout_inherit.riv: artboard "Host" (400x200) has a
// custom style animating 1s through the script. Its "Child" layout (fill)
// inherits, and so does the artboard "Inner" it hosts in a
// NestedArtboardLayout, along with Inner's "InnerChild" layout.
//
// scripted_interpolator_transition_converter.riv: artboard "Main" moves node
// "Mover" from x 0 (state A) to x 100 (state B) through a 1s scripted
// transition, and binds node "Bound"'s x to the view model number "value"
// through a 1s DataConverterInterpolator running the script.

static const float kQuadSteps[] = {0.0f, 6.25f, 25.0f, 56.25f, 100.0f};

TEST_CASE("layouts that inherit a scripted interpolator share the definer's "
          "instance",
          "[scripting]")
{
    auto file = ReadRiveFile("assets/scripted_interpolator_layout_inherit.riv");
    REQUIRE(file != nullptr);
    auto host = file->artboardNamed("Host");
    REQUIRE(host != nullptr);
    host->advance(0.0f);

    auto child = host->find<LayoutComponent>("Child");
    REQUIRE(child != nullptr);
    auto nested = host->find<NestedArtboardLayout>("Inner");
    REQUIRE(nested != nullptr);
    auto inner = nested->artboardInstance();
    REQUIRE(inner != nullptr);
    auto innerChild = inner->find<LayoutComponent>("InnerChild");
    REQUIRE(innerChild != nullptr);

    // The host's style runs a live clone; every layout that inherits it,
    // across the nested artboard too, borrows that same instance.
    auto interpolator = host->interpolator();
    REQUIRE(interpolator != nullptr);
    REQUIRE(interpolator->is<ScriptedInterpolator>());
    CHECK(interpolator->transform(0.5f) == Approx(0.25f));
    CHECK(child->interpolator() == interpolator);
    CHECK(inner->interpolator() == interpolator);
    CHECK(innerChild->interpolator() == interpolator);

    // Widening the host tweens the child's width from 300 to 500 along the
    // script's curve, one quarter of the 1s tween per step.
    CHECK(child->layoutWidth() == Approx(300.0f));
    host->width(600.0f);
    host->advance(0.25f);
    for (float expected : kQuadSteps)
    {
        host->advance(0.25f);
        CHECK(child->layoutWidth() == Approx(300.0f + expected * 2.0f));
    }
}

TEST_CASE("each artboard instance runs its own layout interpolator instance",
          "[scripting]")
{
    auto file = ReadRiveFile("assets/scripted_interpolator_layout_inherit.riv");
    REQUIRE(file != nullptr);
    auto hostA = file->artboardNamed("Host");
    auto hostB = file->artboardNamed("Host");
    REQUIRE(hostA != nullptr);
    REQUIRE(hostB != nullptr);
    hostA->advance(0.0f);
    hostB->advance(0.0f);

    REQUIRE(hostA->interpolator() != nullptr);
    REQUIRE(hostB->interpolator() != nullptr);
    CHECK(hostA->interpolator() != hostB->interpolator());

    auto innerA =
        hostA->find<NestedArtboardLayout>("Inner")->artboardInstance();
    auto innerB =
        hostB->find<NestedArtboardLayout>("Inner")->artboardInstance();
    REQUIRE(innerA != nullptr);
    REQUIRE(innerB != nullptr);
    CHECK(innerA->interpolator() == hostA->interpolator());
    CHECK(innerB->interpolator() == hostB->interpolator());
}

static const ScriptedInterpolator* transitionInterpolator(
    const StateMachine* machine)
{
    auto layer = machine->layer(0);
    for (size_t i = 0; i < layer->stateCount(); i++)
    {
        auto state = layer->state(i);
        for (size_t j = 0; j < state->transitionCount(); j++)
        {
            auto interpolator = state->transition(j)->interpolator();
            if (interpolator != nullptr &&
                interpolator->is<ScriptedInterpolator>())
            {
                return interpolator->as<ScriptedInterpolator>();
            }
        }
    }
    return nullptr;
}

TEST_CASE("a scripted transition runs its state machine instance's clone",
          "[scripting]")
{
    auto file =
        ReadRiveFile("assets/scripted_interpolator_transition_converter.riv");
    REQUIRE(file != nullptr);
    auto artboardA = file->artboardNamed("Main");
    auto artboardB = file->artboardNamed("Main");
    auto machineA = artboardA->stateMachineAt(0);
    auto machineB = artboardB->stateMachineAt(0);

    auto shared = transitionInterpolator(machineA->stateMachine());
    REQUIRE(shared != nullptr);
    auto cloneA = machineA->scriptedObject(shared);
    auto cloneB = machineB->scriptedObject(shared);
    REQUIRE(cloneA != nullptr);
    REQUIRE(cloneB != nullptr);
    CHECK(cloneA != shared);
    CHECK(cloneA != cloneB);

    // Entry -> A on the first step, then A -> B blends over the 1s transition
    // along the script's curve.
    auto mover = artboardA->find<Node>("Mover");
    REQUIRE(mover != nullptr);
    for (float expected : kQuadSteps)
    {
        machineA->advanceAndApply(0.25f);
        CHECK(mover->x() == Approx(expected));
    }
}

TEST_CASE("a scripted converter interpolator runs per bound converter",
          "[scripting]")
{
    auto file =
        ReadRiveFile("assets/scripted_interpolator_transition_converter.riv");
    REQUIRE(file != nullptr);
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto vmi = file->createViewModelInstance(artboard.get());
    auto machine = artboard->stateMachineAt(0);
    machine->bindViewModelInstance(vmi);
    // The converter snaps rather than eases until two timed advances have
    // passed (DataConverterInterpolator::isFirstRun).
    machine->advanceAndApply(0.25f);
    machine->advanceAndApply(0.25f);

    auto bound = artboard->find<Node>("Bound");
    REQUIRE(bound != nullptr);
    CHECK(bound->x() == Approx(0.0f));

    vmi->propertyValue("value")->as<ViewModelInstanceNumber>()->propertyValue(
        100.0f);
    for (float expected : kQuadSteps)
    {
        machine->advanceAndApply(0.25f);
        CHECK(bound->x() == Approx(expected));
    }
}

// scripted_interpolator_converter_inputs.riv runs powered.luau, whose init()
// captures its exponent from a "power" input:
//
//   type Powered = { power: Input<number>, exponent: number }
//   function init(self: Powered, context: Context): boolean
//       self.exponent = self.power
//       return true
//   end
//   function transform(self: Powered, value: number): number
//       return value ^ self.exponent
//   end
//   -- the generator returns { power = 1, exponent = 1, ... }.
//
// Artboard "Main" binds node "Eased"'s x to the view model number "value"
// through a 1s DataConverterInterpolator whose power input is bound to the
// view model's "power", and node "Mapped"'s x through a 0..100 ->
// 0..100 DataConverterRangeMapper whose power input is authored as 2. The view
// model has two instances: "Cubic" (power 3) and "Quad" (power 2).

static void easeValueTo(StateMachineInstance* machine,
                        ViewModelInstance* vmi,
                        Node* node,
                        float target,
                        const float* expected,
                        size_t count)
{
    vmi->propertyValue("value")->as<ViewModelInstanceNumber>()->propertyValue(
        target);
    for (size_t i = 0; i < count; i++)
    {
        machine->advanceAndApply(0.25f);
        CHECK(node->x() == Approx(expected[i]));
    }
}

TEST_CASE("a scripted converter interpolator inits with its bound inputs, "
          "again on rebind",
          "[scripting]")
{
    auto file =
        ReadRiveFile("assets/scripted_interpolator_converter_inputs.riv");
    REQUIRE(file != nullptr);
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto cubic = file->createViewModelInstance(artboard->viewModelId(), 0);
    auto quad = file->createViewModelInstance(artboard->viewModelId(), 1);
    REQUIRE(cubic != nullptr);
    REQUIRE(quad != nullptr);
    auto eased = artboard->find<Node>("Eased");
    REQUIRE(eased != nullptr);

    // init() reads the bound power (3), not the authored 1.
    machine->bindViewModelInstance(cubic);
    // The converter snaps rather than eases until two timed advances have
    // passed (DataConverterInterpolator::isFirstRun).
    machine->advanceAndApply(0.25f);
    machine->advanceAndApply(0.25f);
    const float cubicSteps[] = {0.0f, 1.5625f, 12.5f, 42.1875f, 100.0f};
    easeValueTo(machine.get(), cubic.get(), eased, 100.0f, cubicSteps, 5);

    // Rebinding re-runs init() against the new instance's power (2).
    machine->bindViewModelInstance(quad);
    machine->advanceAndApply(0.25f);
    machine->advanceAndApply(0.25f);
    easeValueTo(machine.get(), quad.get(), eased, 100.0f, kQuadSteps, 5);
}

TEST_CASE("a scripted range mapper runs its own initialized clone",
          "[scripting]")
{
    auto file =
        ReadRiveFile("assets/scripted_interpolator_converter_inputs.riv");
    REQUIRE(file != nullptr);
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard->viewModelId(), 0);
    REQUIRE(vmi != nullptr);
    machine->bindViewModelInstance(vmi);
    machine->advanceAndApply(0.0f);
    auto mapped = artboard->find<Node>("Mapped");
    REQUIRE(mapped != nullptr);

    // The mapper's authored power of 2 only applies once init() has run, so
    // a quarter (not half) of the output range proves the live clone ran.
    auto value = vmi->propertyValue("value")->as<ViewModelInstanceNumber>();
    for (float input : {25.0f, 50.0f, 75.0f})
    {
        value->propertyValue(input);
        machine->advanceAndApply(0.0f);
        CHECK(mapped->x() == Approx(input * input / 100.0f));
    }
}
