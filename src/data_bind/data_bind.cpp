#include "rive/data_bind/data_bind.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/data_bind/bindable_property_artboard.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_list.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/data_bind/bindable_property_integer.hpp"
#include "rive/data_bind/bindable_property_viewmodel.hpp"
#include "rive/data_bind/data_bind_container.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/context/context_value_any.hpp"
#include "rive/data_bind/context/context_value_asset_image.hpp"
#include "rive/data_bind/context/context_value_artboard.hpp"
#include "rive/data_bind/context/context_value_boolean.hpp"
#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/data_bind/context/context_value_trigger.hpp"
#include "rive/data_bind/context/context_value_symbol_list_index.hpp"
#include "rive/data_bind/context/context_value_viewmodel.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/converters/formula/formula_token.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/generated/node_base.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/component.hpp"

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
        initialize();
        auto input = ScriptInput::from(target());
        if (input != nullptr)
        {
            bool ownsDataBind = true;
            if (input->scriptedObject() != nullptr)
            {
                if (input->scriptedObject()->component() != nullptr)
                {
                    auto importer = importStack.latest<ArtboardImporter>(
                        ArtboardBase::typeKey);
                    if (importer != nullptr)
                    {
                        ownsDataBind = false;
                        importer->addDataBind(this);
                    }
                }
                else if (input->scriptedObject()->addDataBindFromScriptedObject(
                             this))
                {
                    ownsDataBind = false;
                }
            }
            input->dataBind(this, ownsDataBind);
        }
        else if (target()->is<DataConverter>())
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
                case BindablePropertyArtboardBase::typeKey:
                case BindablePropertyColorBase::typeKey:
                case BindablePropertyTriggerBase::typeKey:
                case BindablePropertyIntegerBase::typeKey:
                case BindablePropertyAssetBase::typeKey:
                case BindablePropertyViewModelBase::typeKey:
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
                    // Prefer the artboard that actually owns the target object.
                    // Relying on latest<ArtboardImporter> can attach binds to
                    // the wrong source artboard when multiple artboards are
                    // loaded.
                    if (target()->is<Component>())
                    {
                        auto comp = target()->as<Component>();
                        auto parentArtboard = comp->artboard();
                        if (parentArtboard != nullptr)
                        {
                            parentArtboard->addDataBind(this);
                            return Super::import(importStack);
                        }
                    }

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
    if (converter() && converter()->outputType() != DataType::input &&
        converter() && converter()->outputType() != DataType::none)
    {
        return converter()->outputType();
    }
    return sourceOutputType();
}

DataType DataBind::sourceOutputType()
{
    if (m_Source != nullptr)
    {
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
            case ViewModelInstanceArtboardBase::typeKey:
                return DataType::artboard;
            case ViewModelInstanceViewModelBase::typeKey:
                return DataType::viewModel;
        }
    }
    return DataType::none;
}

void DataBind::source(rcp<ViewModelInstanceValue> value)
{
    if (!bindsOnce())
    {
        value->addDependent(this);
    }
    m_Source = value;

    // We treat this as a special case. If an ArtboardComponentList's list
    // property is bound to a number instance, we know that the component
    // will be provided with dettached view model instances that need to be
    // advanced explicitly
    if (m_Source && target() && target()->is<ArtboardComponentList>())
    {
        target()->as<ArtboardComponentList>()->shouldResetInstances(
            m_Source->coreType() == ViewModelInstanceNumberBase::typeKey);
    }
}

void DataBind::clearSource()
{
    if (m_Source != nullptr)
    {

        if (!bindsOnce())
        {
            m_Source->removeDependent(this);
        }
        m_Source = nullptr;
    }
}

DataBind::~DataBind()
{
    unbind();
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
        case DataType::artboard:
            m_ContextValue = new DataBindContextValueArtboard(this);
            break;
        case DataType::viewModel:
            m_ContextValue = new DataBindContextValueViewModel(this);
            break;
        case DataType::any:
            m_ContextValue = new DataBindContextValueAny(this);
            break;
        default:
            break;
    }
    if (m_dataConverter)
    {
        m_dataConverter->reset();
    }
    // if this is a toSource bind on a push-capable target,
    // subscribe to the target's notifyPropertyChanged stream instead of
    // relying on per-frame polling. The container consults targetSupportsPush()
    // to decide whether to also enroll the bind in m_persistingDataBinds.
    //
    // bind() can be called more than once on the same DataBind (e.g. when
    // ArtboardComponentList rebinds list items as the viewmodel changes).
    // Unsubscribe first if we previously subscribed — otherwise
    // addPropertyObserver would prepend the same node twice and form a cycle
    // that makes notifyPropertyChanged loop forever. The Observing flag
    // gates this so we don't deref m_target when we never subscribed (which
    // would be a UAF if the target was freed first).
    if (hasFlag(Flag::Observing) && m_target != nullptr)
    {
        m_target->removePropertyObserver(this);
        setFlag(Flag::Observing, false);
    }
    if (toSource() && m_target != nullptr && targetSupportsPush())
    {
        m_target->addPropertyObserver(this);
        setFlag(Flag::Observing, true);
    }
    addDirt(ComponentDirt::Bindings, true);
}

void DataBind::target(Core* value)
{
    if (m_target == value)
    {
        return;
    }
    // Drop the subscription only when we actually have one — otherwise
    // dereferencing m_target is a UAF if the old target was freed before us
    // (we wouldn't have been notified via onTargetDestroyed because we
    // weren't in its observer list).
    if (hasFlag(Flag::Observing) && m_target != nullptr)
    {
        m_target->removePropertyObserver(this);
        setFlag(Flag::Observing, false);
    }
    m_target = value;
    if (toSource() && m_target != nullptr && targetSupportsPush())
    {
        m_target->addPropertyObserver(this);
        setFlag(Flag::Observing, true);
    }
}

void DataBind::unbind()
{
    clearSource();
    if (hasFlag(Flag::Observing) && m_target != nullptr)
    {
        m_target->removePropertyObserver(this);
        setFlag(Flag::Observing, false);
    }
    if (m_dataConverter != nullptr)
    {
        m_dataConverter->unbind();
    }
    if (m_ContextValue != nullptr)
    {
        delete m_ContextValue;
        m_ContextValue = nullptr;
    }
}

bool DataBind::targetSupportsPush() const
{
    if (m_target == nullptr)
    {
        return false;
    }
    // Push works for any property whose value is exposed via a generated
    // setter that calls notifyPropertyChanged (i.e. anything in *_base.hpp).
    // The following targets are computed/derived and don't fit the push
    // model — leave them on the polling path:
    //
    //   * Solo's activeComponentId — derived from active child, no setter
    //   * BindablePropertyAsset's image — loaded asynchronously, no setter
    //   * BindablePropertyViewModel's value — set via dedicated method
    //   * ViewModelInstanceViewModel's reference — set via dedicated method
    //   * Node's computed* properties — written multiple times per frame by
    //     the transform/layout pipeline; pushing on every intermediate write
    //     triggers dependent binds mid-computation and shifts per-frame
    //     ordering. Polling at end of frame captures only the settled value.
    auto key = propertyKey();
    if (key == SoloBase::activeComponentIdPropertyKey ||
        key == NodeBase::computedLocalXPropertyKey ||
        key == NodeBase::computedLocalYPropertyKey ||
        key == NodeBase::computedWorldXPropertyKey ||
        key == NodeBase::computedWorldYPropertyKey ||
        key == NodeBase::computedRootXPropertyKey ||
        key == NodeBase::computedRootYPropertyKey ||
        key == NodeBase::computedWidthPropertyKey ||
        key == NodeBase::computedHeightPropertyKey ||
        key == ShapeBase::lengthPropertyKey ||
        key == ScrollConstraintBase::scrollIndexPropertyKey ||
        key == ScrollConstraintBase::scrollPercentXPropertyKey ||
        key == ScrollConstraintBase::scrollPercentYPropertyKey ||
        key == ScrollConstraintBase::velocityXPropertyKey ||
        key == ScrollConstraintBase::velocityYPropertyKey ||
        key == ScrollConstraintBase::scrollActivePropertyKey)
    {
        return false;
    }
    auto t = m_target->coreType();
    if (t == BindablePropertyAssetBase::typeKey ||
        t == BindablePropertyViewModelBase::typeKey ||
        t == ViewModelInstanceViewModelBase::typeKey)
    {
        return false;
    }
    return true;
}

bool DataBind::canSkip()
{
    return m_target && m_target->is<Component>() &&
           m_target->as<Component>()->isCollapsed() &&
           propertyKey() != LayoutComponentStyleBase::displayValuePropertyKey;
}

void DataBind::update(ComponentDirt value)
{
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
                // Alternative A: a TwoWay bind subscribes to its target via
                // Core::notifyPropertyChanged. Without suppression, writing
                // the target here would push-notify ourselves and add a
                // spurious dirt entry, causing an extra updateSourceBinding
                // pass in the same frame (silver tests collapse_data_binds-
                // test_2 / virtualize_blendmode regress). Mirror the
                // suppressDirt pattern used by the source-apply path.
                suppressDirt(true);
                m_ContextValue->apply(m_target,
                                      propertyKey(),
                                      (flagsValue & DataBindFlags::Direction) ==
                                          DataBindFlags::ToTarget,
                                      this);
                suppressDirt(false);
            }
        }
    }
}

void DataBind::updateDependents()
{
    if (m_dataConverter)
    {
        m_dataConverter->update();
    }
}

void DataBind::updateSourceBinding(bool invalidate)
{
    if (toSource() && m_target && m_ContextValue != nullptr)
    {
        if (invalidate)
        {
            m_ContextValue->invalidate();
        }
        m_ContextValue->applyToSource(m_target,
                                      propertyKey(),
                                      isMainToSource(),
                                      this);
    }
}

bool DataBind::isMainToSource()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return (flagsValue & DataBindFlags::Direction) == DataBindFlags::ToSource;
}

bool DataBind::sourceToTargetRunsFirst()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return (flagsValue & DataBindFlags::SourceToTargetRunsFirst) ==
           DataBindFlags::SourceToTargetRunsFirst;
}

void DataBind::addDirt(ComponentDirt value, bool recurse)
{
    if (hasFlag(Flag::SuppressDirt) || (m_Dirt & value) == value)
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
    if (enums::is_flag_set(m_Dirt, ComponentDirt::Dependents) &&
        m_ContextValue != nullptr)
    {
        m_ContextValue->invalidate();
    }
    if (m_container && !hasFlag(Flag::Collapsed))
    {
        m_container->addDirtyDataBind(this);
    }
}

void DataBind::container(DataBindContainer* container)
{
    m_container = container;
}

void DataBind::relinkDataBind() { m_container->rebuildDataBind(this); }

bool DataBind::bindsOnce()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return enums::is_flag_set(flagsValue, DataBindFlags::Once);
}

bool DataBind::toSource()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return enums::any_flag_set(flagsValue,
                               DataBindFlags::TwoWay | DataBindFlags::ToSource);
}

bool DataBind::toTarget()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return enums::is_flag_set(flagsValue, DataBindFlags::TwoWay) ||
           !enums::is_flag_set(flagsValue, DataBindFlags::ToSource);
}

bool DataBind::isNameBased()
{
    auto flagsValue = static_cast<DataBindFlags>(flags());
    return enums::is_flag_set(flagsValue, DataBindFlags::NameBased);
}

bool DataBind::advance(float elapsedTime)
{
    if (converter() && m_Source && !hasFlag(Flag::Collapsed))
    {
        return converter()->advance(elapsedTime);
    }
    return false;
}

void DataBind::file(File* value) { m_file = value; };

File* DataBind::file() const { return m_file; };

void DataBind::collapse(bool isCollapsed)
{
    if (hasFlag(Flag::Collapsed) == isCollapsed ||
        propertyKey() == LayoutComponentStyleBase::displayValuePropertyKey ||
        !targetSupportsPush())
    {
        return;
    }
    setFlag(Flag::Collapsed, isCollapsed);
    if (!isCollapsed && m_Dirt != ComponentDirt::None && m_container)
    {
        m_container->addDirtyDataBind(this);
    }
}

void DataBind::initialize()
{
    if (target() && target()->is<Component>())
    {
        target()->as<Component>()->addCollapsable(this);
    }
}