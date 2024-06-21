#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/data_bind_mode.hpp"
#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/artboard.hpp"
#include "rive/generated/core_registry.hpp"
#include <iostream>

using namespace rive;

void DataBindContext::decodeSourcePathIds(Span<const uint8_t> value)
{
    BinaryReader reader(value);
    while (!reader.reachedEnd())
    {
        auto val = reader.readVarUintAs<uint32_t>();
        m_SourcePathIdsBuffer.push_back(val);
    }
}

void DataBindContext::copySourcePathIds(const DataBindContextBase& object)
{
    m_SourcePathIdsBuffer = object.as<DataBindContext>()->m_SourcePathIdsBuffer;
}

void DataBindContext::bind()
{
    auto dataContext = artboard()->dataContext();
    if (dataContext != nullptr)
    {
        auto value = dataContext->getViewModelProperty(m_SourcePathIdsBuffer);
        if (value != nullptr)
        {
            value->addDependent(this);
            m_Source = value;
            Super::bind();
        }
    }
}

void DataBindContext::update(ComponentDirt value)
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

void DataBindContext::updateSourceBinding()
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