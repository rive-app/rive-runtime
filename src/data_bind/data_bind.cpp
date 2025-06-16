#include "rive/data_bind/data_bind.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_list.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/data_bind/bindable_property_integer.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/context/context_value_asset_image.hpp"
#include "rive/data_bind/context/context_value_boolean.hpp"
#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/data_bind/context/context_value_trigger.hpp"
#include "rive/data_bind/context/context_value_symbol_list_index.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/converters/formula/formula_token.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/importers/backboard_importer.hpp"

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
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    file(backboardImporter->file());
    backboardImporter->addDataConverterReferencer(this);
    if (target())
    {
        if (target()->is<DataConverter>())
        {
            target()->as<DataConverter>()->addDataBind(this);
        }
        else if (target()->is<FormulaToken>())
        {
            target()->as<FormulaToken>()->addDataBind(this);
        }
        else
        {
            switch (target()->coreType())
            {
                case BindablePropertyNumberBase::typeKey:
                case BindablePropertyStringBase::typeKey:
                case BindablePropertyBooleanBase::typeKey:
                case BindablePropertyEnumBase::typeKey:
                case BindablePropertyColorBase::typeKey:
                case BindablePropertyTriggerBase::typeKey:
                case BindablePropertyIntegerBase::typeKey:
                case BindablePropertyAssetBase::typeKey:
                case BindablePropertyListBase::typeKey:
                case TransitionPropertyViewModelComparatorBase::typeKey:
                case StateTransitionBase::typeKey:
                {
                    auto stateMachineImporter =
                        importStack.latest<StateMachineImporter>(
                            StateMachineBase::typeKey);
                    if (stateMachineImporter != nullptr)
                    {
                        stateMachineImporter->addDataBind(
                            std::unique_ptr<DataBind>(this));
                        return Super::import(importStack);
                    }
                    break;
                }
                default:
                {
                    auto artboardImporter =
                        importStack.latest<ArtboardImporter>(
                            ArtboardBase::typeKey);
                    if (artboardImporter != nullptr)
                    {
                        artboardImporter->addDataBind(this);
                        return Super::import(importStack);
                    }
                    break;
                }
            }
        }
    }

    return Super::import(importStack);
}

DataType DataBind::outputType()
{
    if (converter())
    {
        return converter()->outputType();
    }
    switch (m_Source->coreType())
    {
        case ViewModelInstanceNumberBase::typeKey:
            return DataType::number;
        case ViewModelInstanceStringBase::typeKey:
            return DataType::string;
        case ViewModelInstanceEnumBase::typeKey:
            return DataType::enumType;
        case ViewModelInstanceColorBase::typeKey:
            return DataType::color;
        case ViewModelInstanceBooleanBase::typeKey:
            return DataType::boolean;
        case ViewModelInstanceListBase::typeKey:
            return DataType::list;
        case ViewModelInstanceTriggerBase::typeKey:
            return DataType::trigger;
        case ViewModelInstanceSymbolListIndexBase::typeKey:
            return DataType::symbolListIndex;
        case ViewModelInstanceAssetImageBase::typeKey:
            return DataType::assetImage;
    }
    return DataType::none;
}

void DataBind::source(ViewModelInstanceValue* value)
{
    if (!bindsOnce())
    {
        value->addDependent(this);
        value->ref();
    }
    m_Source = value;
}

void DataBind::clearSource()
{
    if (m_Source != nullptr)
    {

        if (!bindsOnce())
        {
            m_Source->removeDependent(this);
            m_Source->unref();
        }
        if (m_dataConverter != nullptr)
        {
            m_dataConverter->unbind();
        }
        m_Source = nullptr;
    }
}

DataBind::~DataBind()
{
    clearSource();
    delete m_ContextValue;
    m_ContextValue = nullptr;
    delete m_dataConverter;
    m_dataConverter = nullptr;
}

void DataBind::bind()
{
    delete m_ContextValue;
    m_ContextValue = nullptr;
    switch (outputType())
    {
        case DataType::number:
            m_ContextValue = new DataBindContextValueNumber(this);
            break;
        case DataType::string:
            m_ContextValue = new DataBindContextValueString(this);
            break;
        case DataType::boolean:
            m_ContextValue = new DataBindContextValueBoolean(this);
            break;
        case DataType::color:
            m_ContextValue = new DataBindContextValueColor(this);
            break;
        case DataType::enumType:
            m_ContextValue = new DataBindContextValueEnum(this);
            break;
        case DataType::list:
            m_ContextValue = new DataBindContextValueList(this);
            break;
        case DataType::trigger:
            m_ContextValue = new DataBindContextValueTrigger(this);
            break;
        case DataType::symbolListIndex:
            m_ContextValue = new DataBindContextValueSymbolListIndex(this);
            break;
        case DataType::assetImage:
            m_ContextValue = new DataBindContextValueAssetImage(this);
            break;
        default:
            break;
    }
    addDirt(ComponentDirt::Bindings, true);
}

void DataBind::unbind()
{
    clearSource();
    if (m_ContextValue != nullptr)
    {
        delete m_ContextValue;
        m_ContextValue = nullptr;
    }
}

void DataBind::update(ComponentDirt value)
{
    if ((value & ComponentDirt::Dependents) == ComponentDirt::Dependents &&
        m_dataConverter != nullptr)
    {
        m_dataConverter->update();
    }
    if (m_Source != nullptr && m_ContextValue != nullptr)
    {
        if ((value & ComponentDirt::Bindings) == ComponentDirt::Bindings)
        {
            // TODO: @hernan review how dirt and mode work together. If dirt is
            // not set for certain modes, we might be able to skip the mode
            // validation.
            auto flagsValue = static_cast<DataBindFlags>(flags());
            if (toTarget())
            {
                m_ContextValue->apply(m_target,
                                      propertyKey(),
                                      (flagsValue & DataBindFlags::Direction) ==
                                          DataBindFlags::ToTarget);
            }
        }
    }
}

void DataBind::updateSourceBinding(bool invalidate)
{
    if ((m_Dirt & ComponentDirt::Dependents) == ComponentDirt::Dependents &&
        m_dataConverter != nullptr)
    {
        m_Dirt &= ~ComponentDirt::Dependents;
        m_dataConverter->update();
    }
    auto flagsValue = static_cast<DataBindFlags>(flags());
    if (toSource())
    {
        if (m_ContextValue != nullptr)
        {
            if (invalidate)
            {
                m_ContextValue->invalidate();
            }
            m_ContextValue->applyToSource(
                m_target,
                propertyKey(),
                (flagsValue & DataBindFlags::Direction) ==
                    DataBindFlags::ToSource);
        }
    }
}

void DataBind::addDirt(ComponentDirt value, bool recurse)
{
    if (m_suppressDirt || (m_Dirt & value) == value)
    {
        // Already marked.
        return;
    }

    m_Dirt |= value;
#ifdef WITH_RIVE_TOOLS
    if (m_changedCallback != nullptr)
    {
        m_changedCallback();
    }
#endif
    if (target() != nullptr)
    {
        if (target()->is<DataConverter>())
        {

            target()->as<DataConverter>()->markConverterDirty();
        }
        else if (target()->is<FormulaToken>())
        {

            target()->as<FormulaToken>()->markDirty();
        }
        else if (target()->is<Component>())
        {
            auto artboard = target()->as<Component>()->artboard();
            if (artboard != nullptr)
            {
                artboard->onComponentDirty(target()->as<Component>());
            }
        }
    }
    if ((m_Dirt & ComponentDirt::Dependents) != 0 && m_ContextValue != nullptr)
    {
        m_ContextValue->invalidate();
    }
}

bool DataBind::bindsOnce()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return (flagsValue & DataBindFlags::Once) == DataBindFlags::Once;
}

bool DataBind::toSource()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return (flagsValue & (DataBindFlags::TwoWay | DataBindFlags::ToSource)) !=
           0;
}

bool DataBind::toTarget()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return (flagsValue & DataBindFlags::TwoWay) != 0 ||
           (flagsValue & DataBindFlags::ToSource) == 0;
}

bool DataBind::advance(float elapsedTime)
{
    if (converter())
    {
        return converter()->advance(elapsedTime);
    }
    return false;
}