#include <rive/viewmodel/viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_property_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_value_dependent.hpp>
#include <rive/refcnt.hpp>
#include <catch.hpp>

using namespace rive;

namespace
{
// Minimal value-dependent that records how many times it is asked to relink.
// This mirrors what a scripted property wrapper does: invalidate any cached
// reference to the previous instance.
class RecordingDependent : public ViewModelValueDependent
{
public:
    int relinkCount = 0;
    void relinkDataBind() override { relinkCount++; }
    void addDirt(ComponentDirt, bool) override {}
};

// Builds a parent instance with a single view-model property that references
// `initial`, mirroring how addValue is used in production (clone()).
rcp<ViewModelInstance> makeParentWithViewModelProperty(
    rcp<ViewModelInstance> initial,
    ViewModelInstanceViewModel** outProperty)
{
    auto parent = make_rcp<ViewModelInstance>();
    auto* property = new ViewModelInstanceViewModel();
    property->parentViewModelInstance(parent.get());
    property->referenceViewModelInstance(initial);
    parent->addValue(property);
    *outProperty = property;
    return parent;
}
} // namespace

// Regression: multiple dependents can share the same ViewModelInstanceViewModel
// property (e.g. several scripted property wrappers). Replacing the reference
// must invalidate *all* of them, not just the one that triggered the swap.
TEST_CASE("replaceViewModelByProperty notifies every value dependent",
          "[viewmodel]")
{
    auto instanceA = make_rcp<ViewModelInstance>();
    auto instanceB = make_rcp<ViewModelInstance>();
    ViewModelInstanceViewModel* property = nullptr;
    auto parent = makeParentWithViewModelProperty(instanceA, &property);

    RecordingDependent depA;
    RecordingDependent depB;
    property->addDependent(&depA);
    property->addDependent(&depB);

    bool replaced = parent->replaceViewModelByProperty(property, instanceB);

    REQUIRE(replaced);
    CHECK(property->referenceViewModelInstance() == instanceB);
    CHECK(depA.relinkCount == 1);
    // The sibling dependent must also be invalidated; this is the bug that
    // left a stale cached instance behind.
    CHECK(depB.relinkCount == 1);
}

// The data-bind apply path (updateViewModel) routes through
// replaceViewModelByProperty, so its dependents must be notified too.
TEST_CASE("updateViewModel notifies value dependents", "[viewmodel]")
{
    auto instanceA = make_rcp<ViewModelInstance>();
    auto instanceB = make_rcp<ViewModelInstance>();
    ViewModelInstanceViewModel* property = nullptr;
    auto parent = makeParentWithViewModelProperty(instanceA, &property);

    RecordingDependent dependent;
    property->addDependent(&dependent);

    property->updateViewModel(instanceB.get());

    CHECK(property->referenceViewModelInstance() == instanceB);
    CHECK(dependent.relinkCount == 1);
}

#ifdef WITH_RIVE_TOOLS
namespace
{
int g_changedCount = 0;
void countChanged(ViewModelInstanceViewModel*) { g_changedCount++; }
} // namespace
#endif

// The data-bind apply path calls updateViewModel on every advance without
// comparing first, so re-assigning the instance a property already holds must
// cost nothing: no dependent relink, no re-parent, and (in the editor) no
// changed callback. The editor answers that callback with a full artboard
// re-bind, which resets every data converter -- an interpolator loses its
// in-flight animation and the next value change snaps instead of easing.
TEST_CASE("replaceViewModelByProperty is a no-op when the value is unchanged",
          "[viewmodel]")
{
    auto instanceA = make_rcp<ViewModelInstance>();
    ViewModelInstanceViewModel* property = nullptr;
    auto parent = makeParentWithViewModelProperty(instanceA, &property);

    RecordingDependent dependent;
    property->addDependent(&dependent);

    bool replaced = parent->replaceViewModelByProperty(property, instanceA);

    // Still reports success: the property was found and holds the value asked
    // for. Callers treat false as "no such property".
    REQUIRE(replaced);
    CHECK(property->referenceViewModelInstance() == instanceA);
    CHECK(dependent.relinkCount == 0);
}

#ifdef WITH_RIVE_TOOLS
TEST_CASE("replaceViewModelByProperty only notifies tools on a real swap",
          "[viewmodel]")
{
    auto instanceA = make_rcp<ViewModelInstance>();
    auto instanceB = make_rcp<ViewModelInstance>();
    ViewModelInstanceViewModel* property = nullptr;
    auto parent = makeParentWithViewModelProperty(instanceA, &property);

    property->onChanged(countChanged);
    g_changedCount = 0;

    parent->replaceViewModelByProperty(property, instanceA);
    CHECK(g_changedCount == 0);

    parent->replaceViewModelByProperty(property, instanceB);
    CHECK(g_changedCount == 1);
}
#endif

namespace
{
// Parent instance whose view model exposes a named view-model property, so the
// by-name entry point can be exercised the way a runtime client reaches it.
struct NamedScene
{
    rcp<ViewModel> parentViewModel;
    rcp<ViewModel> childViewModel;
    rcp<ViewModelInstance> parent;
    ViewModelInstanceViewModel* property = nullptr;
};

NamedScene makeNamedScene(rcp<ViewModelInstance> initial)
{
    NamedScene scene;
    scene.childViewModel = make_rcp<ViewModel>();
    scene.parentViewModel = make_rcp<ViewModel>();

    auto* childProperty = new ViewModelPropertyViewModel();
    childProperty->name("child");
    childProperty->viewModelReferenceId(initial->viewModelId());
    scene.parentViewModel->addProperty(childProperty);

    scene.parent = make_rcp<ViewModelInstance>();
    scene.parent->viewModel(scene.parentViewModel.get());

    scene.property = new ViewModelInstanceViewModel();
    scene.property->viewModelProperty(childProperty);
    scene.property->parentViewModelInstance(scene.parent.get());
    scene.property->referenceViewModelInstance(initial);
    scene.parent->addValue(scene.property);
    return scene;
}
} // namespace

// Regression: the by-name API used to carry its own copy of the replace body,
// so it kept re-parenting and relinking when handed the instance already held.
// Identical operations must not behave differently depending on which entry
// point the caller used.
TEST_CASE("replaceViewModelByName is a no-op when the value is unchanged",
          "[viewmodel]")
{
    auto instanceA = make_rcp<ViewModelInstance>();
    instanceA->viewModelId(7);
    auto scene = makeNamedScene(instanceA);

    RecordingDependent dependent;
    scene.property->addDependent(&dependent);

    bool replaced = scene.parent->replaceViewModelByName("child", instanceA);

    REQUIRE(replaced);
    CHECK(scene.property->referenceViewModelInstance() == instanceA);
    CHECK(dependent.relinkCount == 0);
}

TEST_CASE("replaceViewModelByName still swaps a different instance",
          "[viewmodel]")
{
    auto instanceA = make_rcp<ViewModelInstance>();
    instanceA->viewModelId(7);
    auto instanceB = make_rcp<ViewModelInstance>();
    instanceB->viewModelId(7);
    auto scene = makeNamedScene(instanceA);

    RecordingDependent dependent;
    scene.property->addDependent(&dependent);

    bool replaced = scene.parent->replaceViewModelByName("child", instanceB);

    REQUIRE(replaced);
    CHECK(scene.property->referenceViewModelInstance() == instanceB);
    CHECK(dependent.relinkCount == 1);
}
