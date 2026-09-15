#include <catch.hpp>
#include "scripting_test_utilities.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

using namespace rive;

namespace
{
// Builds a ViewModelInstance carrying a single trigger property. The trigger is
// returned via `outTrigger` so the test can set/read its value; the instance
// owns it. A triggered value is reset to 0 by ViewModelInstance::advanced().
rcp<ViewModelInstance> makeInstanceWithTrigger(
    ViewModelInstanceTrigger** outTrigger)
{
    auto instance = make_rcp<ViewModelInstance>();
    auto* trigger = new ViewModelInstanceTrigger();
    instance->addValue(trigger);
    *outTrigger = trigger;
    return instance;
}
} // namespace

// A ScriptedViewModel whose instance is detached from the bound tree (no
// parents) must have its instance advanced by advanceDetachedViewModels(),
// while one whose instance has a parent is skipped (it is already advanced
// through the tree). Advancing consumes the trigger (resets it to 0), so we
// assert on that.
TEST_CASE("advanceDetachedViewModels advances only parentless instances",
          "[scripting]")
{
    ScriptingTest test("return 1");
    auto* context = test.vm()->context();
    auto* L = test.state();

    // Detached: no parents.
    ViewModelInstanceTrigger* detachedTrigger = nullptr;
    auto detached = makeInstanceWithTrigger(&detachedTrigger);

    // Attached: referenced by a parent, which calls addParent under the hood.
    ViewModelInstanceTrigger* attachedTrigger = nullptr;
    auto attached = makeInstanceWithTrigger(&attachedTrigger);
    auto parent = make_rcp<ViewModelInstance>();
    auto* vmProperty = new ViewModelInstanceViewModel();
    vmProperty->parentViewModelInstance(parent.get());
    vmProperty->referenceViewModelInstance(attached);
    parent->addValue(vmProperty);

    REQUIRE_FALSE(detached->hasParents());
    REQUIRE(attached->hasParents());

    {
        // Constructing a ScriptedViewModel registers it with the context via
        // the lua thread data; destruction unregisters it.
        ScriptedViewModel scriptedDetached(L, nullptr, detached);
        ScriptedViewModel scriptedAttached(L, nullptr, attached);

        detachedTrigger->trigger();
        attachedTrigger->trigger();
        REQUIRE(detachedTrigger->propertyValue() == 1);
        REQUIRE(attachedTrigger->propertyValue() == 1);

        context->advanceDetachedViewModels();

        // Detached root advanced -> trigger consumed.
        CHECK(detachedTrigger->propertyValue() == 0);
        // Parented instance skipped -> trigger retained (it is advanced through
        // the bound tree instead).
        CHECK(attachedTrigger->propertyValue() == 1);
    }

    // Both scripted view models are now destroyed and untracked; advancing must
    // not touch dangling pointers.
    context->advanceDetachedViewModels();
}

// A ScriptedViewModel that wraps no instance (property-less) registers nothing
// and must be handled safely during advance.
TEST_CASE("advanceDetachedViewModels tolerates null instances", "[scripting]")
{
    ScriptingTest test("return 1");
    auto* context = test.vm()->context();
    auto* L = test.state();

    ScriptedViewModel scriptedNull(L, nullptr, nullptr);
    context->advanceDetachedViewModels();
}

// The core lifetime fix: tracking is keyed to owner lifetime, not to the Lua
// wrapper's GC. An instance owned by more than one registrant (e.g. a scripted
// artboard AND a ScriptedViewModel wrapper) must stay tracked — and keep being
// advanced — until the LAST owner goes away, even if the script drops its
// wrapper first.
TEST_CASE("tracked instance outlives its wrapper while another owner holds it",
          "[scripting]")
{
    ScriptingTest test("return 1");
    auto* context = test.vm()->context();
    auto* L = test.state();

    ViewModelInstanceTrigger* trigger = nullptr;
    auto instance = makeInstanceWithTrigger(&trigger);
    REQUIRE_FALSE(instance->hasParents());

    // A second owner (simulating a ScriptReffedArtboard) registers the instance
    // directly, independent of any Lua wrapper.
    context->trackViewModelInstance(instance);

    {
        // A ScriptedViewModel wrapper is a second registrant (registrations 2).
        ScriptedViewModel wrapper(L, nullptr, instance);
        trigger->trigger();
        context->advanceDetachedViewModels();
        CHECK(trigger->propertyValue() == 0); // advanced
    }

    // Wrapper destroyed (registrations 2 -> 1). The instance is still owned by
    // the simulated artboard, so it must STILL be advanced. This is the bug the
    // fix addresses: previously it would have been untracked here.
    trigger->trigger();
    context->advanceDetachedViewModels();
    CHECK(trigger->propertyValue() == 0);

    // Last owner releases it (registrations 1 -> 0): no longer tracked, so it
    // is not advanced (and nothing dangles).
    context->untrackViewModelInstance(instance.get());
    trigger->trigger();
    context->advanceDetachedViewModels();
    CHECK(trigger->propertyValue() == 1);
}
