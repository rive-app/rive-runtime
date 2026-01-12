#include "rive/data_bind/context/context_value_artboard.hpp"
#include "rive/data_bind/data_values/data_value_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/file.hpp"
#include "rive/artboard_referencer.hpp"

using namespace rive;

DataBindContextValueArtboard::DataBindContextValueArtboard(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueArtboard::apply(Core* target,
                                         uint32_t propertyKey,
                                         bool isMainDirection)
{
    syncSourceValue();
    auto source = m_dataBind->source();
    if (source != nullptr && source->is<ViewModelInstanceArtboard>())
    {
        auto artboardReferencer = ArtboardReferencer::from(target);
        if (artboardReferencer)
        {

            artboardReferencer->updateArtboard(
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