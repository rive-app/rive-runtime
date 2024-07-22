#include "rive/data_bind/data_bind.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/context/context_value_boolean.hpp"
#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"

using namespace rive;

StatusCode DataBind::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    return StatusCode::Ok;
}

StatusCode DataBind::import(ImportStack& importStack)
{
    if (target())
    {
        switch (target()->coreType())
        {
            case BindablePropertyNumberBase::typeKey:
            case BindablePropertyStringBase::typeKey:
            case BindablePropertyBooleanBase::typeKey:
            case BindablePropertyEnumBase::typeKey:
            case BindablePropertyColorBase::typeKey:
            {
                auto stateMachineImporter =
                    importStack.latest<StateMachineImporter>(StateMachineBase::typeKey);
                if (stateMachineImporter != nullptr)
                {
                    stateMachineImporter->addDataBind(std::unique_ptr<DataBind>(this));
                    return Super::import(importStack);
                }
                break;
            }
            default:
            {
                auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
                if (artboardImporter != nullptr)
                {
                    artboardImporter->addDataBind(this);
                    return Super::import(importStack);
                }
                break;
            }
        }
    }
    return Super::import(importStack);
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
            auto flagsValue = static_cast<DataBindFlags>(flags());
            if (((flagsValue & DataBindFlags::Direction) == DataBindFlags::ToTarget) ||
                ((flagsValue & DataBindFlags::TwoWay) == DataBindFlags::TwoWay))
            {
                m_ContextValue->apply(m_target, propertyKey());
            }
        }
    }
}

void DataBind::updateSourceBinding()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    if (((flagsValue & DataBindFlags::Direction) == DataBindFlags::ToSource) ||
        ((flagsValue & DataBindFlags::TwoWay) == DataBindFlags::TwoWay))
    {
        if (m_ContextValue != nullptr)
        {
            m_ContextValue->applyToSource(m_target, propertyKey());
        }
    }
}

bool DataBind::addDirt(ComponentDirt value, bool recurse)
{
    if ((m_Dirt & value) == value)
    {
        // Already marked.
        return false;
    }

    m_Dirt |= value;
    return true;
}