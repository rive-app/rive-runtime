#include "rive/data_bind_flags.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
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

void DataBindContext::bindFromContext(DataContext* dataContext)
{
    if (dataContext != nullptr)
    {
        auto vmSource =
            dataContext->getViewModelProperty(m_SourcePathIdsBuffer);
        if (vmSource != m_Source)
        {
            if (vmSource != nullptr)
            {
                clearSource();
                source(vmSource);
                bind();
            }
            else
            {
                unbind();
            }
        }
        else
        {
            addDirt(ComponentDirt::Bindings, true);
        }
        if (m_dataConverter != nullptr)
        {
            m_dataConverter->bindFromContext(dataContext, this);
        }
    }
}
