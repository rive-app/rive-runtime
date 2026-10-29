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

DataValue* DataConverterOperationViewModel::convert(
    DataValue* input,
    DataBind* dataBind)
{
  return convertValue(input, resolveValue(dataBind));
}

DataValue* DataConverterOperationViewModel::reverseConvert(
    DataValue* input,
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
  m_SourcePathIdsBuffer = object.as<DataConverterOperationViewModel>()
                              ->m_SourcePathIdsBuffer;
}

DataConverterOperationViewModel::~DataConverterOperationViewModel()
{
  clearSource();
}

void DataConverterOperationViewModel::clearSource()
{
  if (m_source != nullptr)
  {
    m_source->removeDependent(this);
    m_source = nullptr;
  }
}

void DataConverterOperationViewModel::bindFromContext(
    DataContext* dataContext,
    DataBind* dataBind)
{
  DataConverter::bindFromContext(dataContext, dataBind);
  ViewModelInstanceNumber* source = nullptr;
  if (dataContext != nullptr)
  {
    auto propertyValue = dataContext->getViewModelProperty(
        m_SourcePathIdsBuffer);
    if (propertyValue != nullptr &&
        propertyValue->is<ViewModelInstanceNumber>())
    {
      source = propertyValue->as<ViewModelInstanceNumber>();
    }
  }
  if (source == m_source.get())
  {
    return;
  }
  // Leave the previous operand, or it keeps pointing at this converter after
  // the converter is deleted.
  clearSource();
  m_source = ref_rcp(source);
  if (m_source != nullptr)
  {
    m_source->addDependent(this);
  }
}

void DataConverterOperationViewModel::unbind()
{
  clearSource();
  DataConverter::unbind();
}

// Registered as itself rather than as the bind: the operand can also be the
// bind's source or another converter's operand, and the dependents list holds
// each pointer once, so taking the bind off would drop their subscription.
void DataConverterOperationViewModel::addDirt(ComponentDirt value, bool recurse)
{
  if (m_parentDataBind != nullptr)
  {
    m_parentDataBind->addDirt(value, recurse);
  }
}