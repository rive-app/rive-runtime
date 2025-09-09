#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_operation_viewmodel.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_bind_context.hpp"

using namespace rive;

float DataConverterOperationViewModel::resolveValue(DataBind* dataBind)
{

    if (m_source != nullptr)
    {
        return m_source->propertyValue();
    }
    return 0.0f;
}

DataValue* DataConverterOperationViewModel::convert(DataValue* input,
                                                    DataBind* dataBind)
{
    return convertValue(input, resolveValue(dataBind));
}

DataValue* DataConverterOperationViewModel::reverseConvert(DataValue* input,
                                                           DataBind* dataBind)
{
    return reverseConvertValue(input, resolveValue(dataBind));
}

void DataConverterOperationViewModel::decodeSourcePathIds(
    Span<const uint8_t> value)
{
    BinaryReader reader(value);
    while (!reader.reachedEnd())
    {
        auto val = reader.readVarUintAs<uint32_t>();
        m_SourcePathIdsBuffer.push_back(val);
    }
}

void DataConverterOperationViewModel::copySourcePathIds(
    const DataConverterOperationViewModelBase& object)
{
    m_SourcePathIdsBuffer =
        object.as<DataConverterOperationViewModel>()->m_SourcePathIdsBuffer;
}

void DataConverterOperationViewModel::bindFromContext(DataContext* dataContext,
                                                      DataBind* dataBind)
{
    DataConverter::bindFromContext(dataContext, dataBind);
    auto propertyValue =
        dataContext->getViewModelProperty(m_SourcePathIdsBuffer);
    if (propertyValue != nullptr &&
        propertyValue->is<ViewModelInstanceNumber>())
    {
        m_source = propertyValue->as<ViewModelInstanceNumber>();
        m_source->addDependent(dataBind);
    }
}