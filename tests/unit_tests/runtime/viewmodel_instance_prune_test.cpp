#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/refcnt.hpp"
#include <catch.hpp>

using namespace rive;

namespace
{
// Builds a number value with the given runtime property id and value.
ViewModelInstanceNumber* makeNumber(uint32_t propertyId, float value)
{
    auto* number = new ViewModelInstanceNumber();
    number->viewModelPropertyId(propertyId);
    number->propertyValue(value);
    return number;
}

// Builds an instance for view model `viewModelId` holding the given numbers.
rcp<ViewModelInstance> makeInstance(
    uint32_t viewModelId,
    std::vector<ViewModelInstanceNumber*> numbers)
{
    auto instance = make_rcp<ViewModelInstance>();
    instance->viewModelId(viewModelId);
    for (auto* number : numbers)
    {
        instance->addValue(number);
    }
    return instance;
}
} // namespace

TEST_CASE("removeValue removes only the matching property",
          "[viewmodel_instance]")
{
    auto instance =
        makeInstance(7, {makeNumber(100, 1.0f), makeNumber(200, 2.0f)});

    REQUIRE(instance->propertyValue(100u) != nullptr);
    REQUIRE(instance->propertyValue(200u) != nullptr);

    // Removing an absent id reports nothing removed and leaves both values.
    CHECK(instance->removeValue(999u) == false);
    CHECK(instance->propertyValue(100u) != nullptr);
    CHECK(instance->propertyValue(200u) != nullptr);

    // Removing a present id reports success and drops only that value.
    CHECK(instance->removeValue(200u) == true);
    CHECK(instance->propertyValue(200u) == nullptr);
    CHECK(instance->propertyValue(100u) != nullptr);
}

TEST_CASE("Sparse override instance falls through to parent for non-overridden "
          "properties",
          "[viewmodel_instance]")
{
    // Parent context: both properties resolve to their parent values.
    auto parentInstance =
        makeInstance(7, {makeNumber(100, 10.0f), makeNumber(200, 20.0f)});
    auto parentContext = make_rcp<DataContext>(parentInstance);

    // Child (override) context: a full copy that is then pruned down to only
    // property 100 (the explicitly-overridden property), matching what the
    // editor does for a sparse nested-artboard override instance.
    auto childInstance =
        makeInstance(7, {makeNumber(100, 99.0f), makeNumber(200, 88.0f)});
    childInstance->removeValue(200u); // prune the non-overridden property

    auto childContext = make_rcp<DataContext>(childInstance);
    childContext->parent(parentContext);

    // Property 100 is present on the override → resolves to the override value.
    auto* resolvedA = childContext->getViewModelProperty({7u, 100u});
    REQUIRE(resolvedA != nullptr);
    CHECK(resolvedA->as<ViewModelInstanceNumber>()->propertyValue() ==
          Approx(99.0f));

    // Property 200 was pruned → resolution must fall through to the parent
    // value instead of returning the override's (default) copy. This is the
    // regression guard: before the fix the override carried every property and
    // this returned 88.0f, leaking the override onto a non-overridden property.
    auto* resolvedB = childContext->getViewModelProperty({7u, 200u});
    REQUIRE(resolvedB != nullptr);
    CHECK(resolvedB->as<ViewModelInstanceNumber>()->propertyValue() ==
          Approx(20.0f));
}
