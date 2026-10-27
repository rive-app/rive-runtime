#include "rive/data_bind/context/context_value_artboard.hpp"
#include "rive/data_bind/data_values/data_value_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/file.hpp"
#include "rive/artboard_referencer.hpp"
#include "rive/text/text_value_run.hpp"

using namespace rive;

DataBindContextValueArtboard::DataBindContextValueArtboard(DataBind* dataBind)
    : DataBindContextValue(dataBind)
{}

void DataBindContextValueArtboard::apply(
    Core* target,
    uint32_t propertyKey,
    bool isMainDirection,
    DataBind* dataBind)
{
  syncSourceValue(dataBind);
  auto source = dataBind->source();
  if (source != nullptr && source->is<ViewModelInstanceArtboard>())
  {
    // A run's styleId bound to a text style resolves the style the value
    // names, not an object id.
    if (target->is<TextValueRun>() &&
        propertyKey == TextValueRunBase::styleIdPropertyKey)
    {
      target->as<TextValueRun>()->bindStyle(
          source->as<ViewModelInstanceArtboard>());
      return;
    }
    auto artboardReferencer = ArtboardReferencer::from(target);
    if (artboardReferencer)
    {

      artboardReferencer->updateArtboard(
          source->as<ViewModelInstanceArtboard>());
    }
    else
    {
      auto value = calculateValue<DataValueArtboard, uint32_t>(
          m_dataValue,
          isMainDirection,
          dataBind);
      CoreRegistry::setUint(target, propertyKey, value);
    }
  }
}