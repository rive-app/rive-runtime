#include "rive/importers/scripted_object_importer.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/script_input_viewmodel_property.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_property_enum_custom.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"

using namespace rive;

void ScriptInputViewModelProperty::decodeDataBindPathIds(
    Span<const uint8_t> value)
{
    BinaryReader reader(value);
    while (!reader.reachedEnd())
    {
        auto val = reader.readVarUintAs<uint32_t>();
        m_DataBindPathIdsBuffer.push_back(val);
    }
}

void ScriptInputViewModelProperty::copyDataBindPathIds(
    const ScriptInputViewModelPropertyBase& object)
{
    m_DataBindPathIdsBuffer =
        object.as<ScriptInputViewModelProperty>()->m_DataBindPathIdsBuffer;
}

void ScriptInputViewModelProperty::initScriptedValue()
{
    ScriptInput::initScriptedValue();
    if (m_viewModelInstanceValue == nullptr)
    {
        return;
    }
    auto obj = scriptedObject();
    if (obj)
    {
        obj->setViewModelInput(name(), m_viewModelInstanceValue);
    }
}

bool ScriptInputViewModelProperty::validateForScriptInit()
{
    m_viewModelInstanceValue = nullptr;
    auto dataContext = artboard()->dataContext();
    if (dataContext == nullptr)
    {
        return false;
    }
    auto instanceValue = dataContext->getViewModelProperty(dataBindPathIds());
    if (instanceValue == nullptr)
    {
        return false;
    }
    auto viewModelProperty = instanceValue->viewModelProperty();
    if (viewModelProperty == nullptr)
    {
        return false;
    }
    m_viewModelInstanceValue = instanceValue;
    return true;
}

StatusCode ScriptInputViewModelProperty::import(ImportStack& importStack)
{
    auto importer =
        importStack.latest<ScriptedObjectImporter>(ScriptedDrawable::typeKey);
    if (importer == nullptr)
    {
        return StatusCode::MissingObject;
    }
    importer->addInput(this);

    return Super::import(importStack);
}