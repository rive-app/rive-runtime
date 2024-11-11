#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_operation_viewmodel.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/data_bind/data_bind_context.hpp"

using namespace rive;

float DataConverterOperationViewModel::resolveValue(DataBind* dataBind)
{

    if (dataBind->is<DataBindContext>())
    {
        // We dynamically load the property value every time we apply the
        // converter. This shouldn't be too expensive since even with a long
        // path, at each level it uses an index to fetch it from the vector. But
        // if we find that this becomes a bottleneck, we might need to resolve
        // them at the data bind level on initialization and pass them as an
        // argument.
        auto dataContext = dataBind->as<DataBindContext>()->dataContext();
        auto propertyValue =
            dataContext->getViewModelProperty(m_SourcePathIdsBuffer);
        if (propertyValue != nullptr &&
            propertyValue->is<ViewModelInstanceNumber>())
        {
            auto value =
                propertyValue->as<ViewModelInstanceNumber>()->propertyValue();
            return value;
        }
    }
    return 0;
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