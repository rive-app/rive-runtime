#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_mode.hpp"
#include "rive/artboard.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/context/context_value_boolean.hpp"
#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/context/context_value_color.hpp"

using namespace rive;

// StatusCode DataBind::onAddedClean(CoreContext* context)
// {
//     return Super::onAddedClean(context);
// }

StatusCode DataBind::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto coreObject = context->resolve(targetId());
    if (coreObject == nullptr || !coreObject->is<Component>())
    {
        return StatusCode::MissingObject;
    }

    m_target = static_cast<Component*>(coreObject);

    return StatusCode::Ok;
}

StatusCode DataBind::import(ImportStack& importStack) { return Super::import(importStack); }

void DataBind::buildDependencies()
{
    Super::buildDependencies();
    auto mode = static_cast<DataBindMode>(modeValue());
    if (mode == DataBindMode::oneWayToSource || mode == DataBindMode::twoWay)
    {
        m_target->addDependent(this);
    }
}

void DataBind::bind()
{
    switch (m_Source->coreType())
    {
        case ViewModelInstanceNumberBase::typeKey:
            m_ContextValue = rivestd::make_unique<DataBindContextValueNumber>(m_Source);
            break;
        case ViewModelInstanceStringBase::typeKey:
            m_ContextValue = rivestd::make_unique<DataBindContextValueString>(m_Source);
            break;
        case ViewModelInstanceEnumBase::typeKey:
            m_ContextValue = rivestd::make_unique<DataBindContextValueEnum>(m_Source);
            break;
        case ViewModelInstanceListBase::typeKey:
            m_ContextValue = rivestd::make_unique<DataBindContextValueList>(m_Source);
            m_ContextValue->update(m_target);
            break;
        case ViewModelInstanceColorBase::typeKey:
            m_ContextValue = rivestd::make_unique<DataBindContextValueColor>(m_Source);
            break;
        case ViewModelInstanceBooleanBase::typeKey:
            m_ContextValue = rivestd::make_unique<DataBindContextValueBoolean>(m_Source);
            break;
    }
}

void DataBind::update(ComponentDirt value)
{
    if (m_Source != nullptr && m_ContextValue != nullptr)
    {

        // Use the ComponentDirt::Components flag to indicate the viewmodel has added or removed
        // an element to a list.
        if ((value & ComponentDirt::Components) == ComponentDirt::Components)
        {
            m_ContextValue->update(m_target);
        }
        if ((value & ComponentDirt::Bindings) == ComponentDirt::Bindings)
        {
            // TODO: @hernan review how dirt and mode work together. If dirt is not set for
            // certain modes, we might be able to skip the mode validation.
            auto mode = static_cast<DataBindMode>(modeValue());
            if (mode == DataBindMode::oneWay || mode == DataBindMode::twoWay)
            {
                m_ContextValue->apply(m_target, propertyKey());
            }
        }
    }
    Super::update(value);
}

void DataBind::updateSourceBinding()
{
    auto mode = static_cast<DataBindMode>(modeValue());
    if (mode == DataBindMode::oneWayToSource || mode == DataBindMode::twoWay)
    {
        if (m_ContextValue != nullptr)
        {
            m_ContextValue->applyToSource(m_target, propertyKey());
        }
    }
}