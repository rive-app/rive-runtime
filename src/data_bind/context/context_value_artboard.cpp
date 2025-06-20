#include "rive/data_bind/context/context_value_artboard.hpp"
#include "rive/data_bind/data_values/data_value_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/file.hpp"

using namespace rive;

DataBindContextValueArtboard::DataBindContextValueArtboard(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueArtboard::apply(Core* target,
                                         uint32_t propertyKey,
                                         bool isMainDirection)
{
    auto source = m_dataBind->source();
    if (target->is<NestedArtboard>() && source != nullptr &&
        source->is<ViewModelInstanceArtboard>())
    {
        target->as<NestedArtboard>()->updateArtboard(
            source->as<ViewModelInstanceArtboard>());
    }
}

bool DataBindContextValueArtboard::syncTargetValue(Core* target,
                                                   uint32_t propertyKey)
{
    auto value = CoreRegistry::getUint(target, propertyKey);

    if (m_previousValue != value)
    {
        m_previousValue = value;
        m_targetDataValue.value(value);
        return true;
    }
    return false;
}