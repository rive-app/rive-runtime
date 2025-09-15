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
    if (source != nullptr && source->is<ViewModelInstanceArtboard>())
    {
        if (target->is<NestedArtboard>())
        {

            target->as<NestedArtboard>()->updateArtboard(
                source->as<ViewModelInstanceArtboard>());
        }
        else
        {
            auto value =
                calculateValue<DataValueArtboard, uint32_t>(m_dataValue,
                                                            isMainDirection,
                                                            m_dataBind);
            CoreRegistry::setUint(target, propertyKey, value);
        }
    }
}