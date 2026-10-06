#include "rive/data_bind/context/context_value_trigger.hpp"
#include "rive/custom_property_trigger.hpp"
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"

using namespace rive;

DataBindContextValueTrigger::DataBindContextValueTrigger(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

static bool isTrigger(Core* object)
{
    return object->is<CustomPropertyTrigger>() ||
           object->is<ViewModelInstanceTrigger>() ||
           object->is<BindablePropertyTrigger>();
}

void DataBindContextValueTrigger::apply(Core* target,
                                        uint32_t propertyKey,
                                        bool isMainDirection,
                                        DataBind* dataBind)
{
    syncSourceValue(dataBind);
    if (!isTrigger(target))
    {
        CoreRegistry::setUint(
            target,
            propertyKey,
            calculateValue<DataValueTrigger, uint32_t>(m_dataValue,
                                                       isMainDirection,
                                                       dataBind));
        return;
    }
    // The source's own count, ahead of any converter, which is what
    // applyToSource records too.
    uint32_t count =
        m_dataValue != nullptr && m_dataValue->is<DataValueTrigger>()
            ? m_dataValue->as<DataValueTrigger>()->value()
            : calculateValue<DataValueTrigger, uint32_t>(m_dataValue,
                                                         isMainDirection,
                                                         dataBind);
    // The first look, as the bind is made, only notes where it stands.
    if (m_sourceCountKnown && count != m_sourceCount)
    {
        CoreRegistry::setUint(target,
                              propertyKey,
                              CoreRegistry::getUint(target, propertyKey) + 1);
    }
    m_sourceCount = count;
    m_sourceCountKnown = true;
    // DataBind::update syncs the cached target value right after this.
    m_targetCountKnown = true;
}

void DataBindContextValueTrigger::applyToSource(Core* target,
                                                uint32_t propertyKey,
                                                bool isMainDirection,
                                                DataBind* dataBind)
{
    auto source = dataBind->source();
    if (source == nullptr || !source->is<ViewModelInstanceTrigger>())
    {
        DataBindContextValue::applyToSource(target,
                                            propertyKey,
                                            isMainDirection,
                                            dataBind);
        return;
    }
    // A listener action writing this bind fires the source, and so does the
    // target firing (its count changing). Settling the cached target value
    // the first time doesn't.
    bool targetFired =
        m_targetValue.syncTargetValue(dataBind) && m_targetCountKnown;
    m_targetCountKnown = true;
    bool requested = m_sourceWriteRequested;
    m_sourceWriteRequested = false;
    m_isValid = true;
    if (!targetFired && !requested)
    {
        return;
    }
    auto trigger = source->as<ViewModelInstanceTrigger>();
    dataBind->suppressDirt(true);
    trigger->trigger();
    dataBind->suppressDirt(false);
    // So that apply doesn't relay this fire back to the target.
    m_sourceCount = trigger->propertyValue();
    m_sourceCountKnown = true;
}
