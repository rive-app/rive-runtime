#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
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
