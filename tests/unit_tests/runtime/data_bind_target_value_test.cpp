#include <catch.hpp>
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/data_bind/context/context_target_value.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

using namespace rive;

// A trigger listener whose bind path lands on a view model reference: the
// source makes initialize() pick a view model value, while the trigger's uint
// property syncs as an integer. The sync used to write that integer through
// the view model value, over the ViewModelInstance pointer it holds, which the
// next to-source apply then dereferenced.
TEST_CASE("a uint target never writes through a view model value",
          "[data_bind]")
{
    auto source = make_rcp<ViewModelInstanceViewModel>();
    BindablePropertyTrigger target;
    target.propertyValue(107);
    DataBind dataBind;
    dataBind.propertyKey(BindablePropertyTriggerBase::propertyValuePropertyKey);
    dataBind.source(source);
    dataBind.target(&target);

    DataBindContextTargetValue targetValue;
    targetValue.initialize(&dataBind);
    REQUIRE(targetValue.dataValue() != nullptr);
    REQUIRE(targetValue.dataValue()->is<DataValueViewModel>());

    CHECK_FALSE(targetValue.syncTargetValue(&dataBind));
    CHECK(targetValue.dataValue()->as<DataValueViewModel>()->value() ==
          nullptr);

    dataBind.target(nullptr);
}
