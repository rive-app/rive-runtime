#include "rive/animation/transition_property_artboard_comparator.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"
#include "rive/animation/artboard_property.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/bindable_property_number.hpp"

using namespace rive;

float TransitionPropertyArtboardComparator::propertyValue(
    const StateMachineInstance* stateMachineInstance)
{
    auto artboard = stateMachineInstance->artboard();
    if (artboard != nullptr)
    {

        auto property = static_cast<ArtboardProperty>(propertyType());
        switch (property)
        {
            case ArtboardProperty::width:
                return artboard->layoutWidth();
                break;
            case ArtboardProperty::height:
                return artboard->layoutHeight();
                break;
            case ArtboardProperty::ratio:
                return artboard->layoutWidth() / artboard->layoutHeight();
                break;

            default:
                break;
        }
    }
    return 0;
}

bool TransitionPropertyArtboardComparator::compare(TransitionComparator* comparand,
                                                   TransitionConditionOp operation,
                                                   const StateMachineInstance* stateMachineInstance)
{
    auto value = propertyValue(stateMachineInstance);
    if (comparand->is<TransitionPropertyViewModelComparator>())
    {
        auto rightValue = comparand->as<TransitionPropertyViewModelComparator>()
                              ->value<BindablePropertyNumber, float>(stateMachineInstance);
        return compareNumbers(value, rightValue, operation);
    }
    else if (comparand->is<TransitionValueNumberComparator>())
    {
        auto rightValue = comparand->as<TransitionValueNumberComparator>()->value();
        return compareNumbers(value, rightValue, operation);
    }
    return false;
}