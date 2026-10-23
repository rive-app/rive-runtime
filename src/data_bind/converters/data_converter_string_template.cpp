#include "rive/data_bind/converters/data_converter_string_template.hpp"

using namespace rive;

DataConverterStringTemplate::~DataConverterStringTemplate()
{
  // Unbind first so the binds stop observing the arguments before they're
  // deleted. The binds themselves are deleted by ~DataConverter.
  unbind();
  for (auto argument : m_arguments)
  {
    delete argument;
  }
  m_arguments.clear();
}

void DataConverterStringTemplate::addArgument(StringTemplateArgument* argument)
{
  argument->stringTemplate(this);
  m_arguments.push_back(argument);
}

StringTemplateArgument* DataConverterStringTemplate::argument(
    const std::string& name) const
{
  for (auto argument : m_arguments)
  {
    if (argument->name() == name)
    {
      return argument;
    }
  }
  return nullptr;
}

void DataConverterStringTemplate::openDelimiterChanged()
{
  m_segmentsDirty = true;
  markConverterDirty();
}

void DataConverterStringTemplate::closeDelimiterChanged()
{
  m_segmentsDirty = true;
  markConverterDirty();
}

DataValue* DataConverterStringTemplate::convert(
    DataValue* input,
    DataBind* dataBind)
{
  if (input == nullptr || !input->is<DataValueString>())
  {
    m_output.value("");
    return &m_output;
  }
  const std::string& text = input->as<DataValueString>()->value();
  if (m_segmentsDirty || text != m_parsedText)
  {
    StringTemplateFormat::parse(
        text,
        openDelimiter(),
        closeDelimiter(),
        m_segments);
    m_parsedText = text;
    m_segmentsDirty = false;
  }
  std::string output;
  for (const auto& segment : m_segments)
  {
    StringTemplateArgument* placeholderArgument = nullptr;
    switch (segment.kind)
    {
      case StringTemplateSegment::Kind::literal:
        output += segment.text;
        continue;
      case StringTemplateSegment::Kind::name:
        placeholderArgument = argument(segment.name);
        break;
      case StringTemplateSegment::Kind::position:
      case StringTemplateSegment::Kind::next:
        if (segment.index < m_arguments.size())
        {
          placeholderArgument = m_arguments[segment.index];
        }
        break;
    }
    // Placeholders without an argument write nothing.
    if (placeholderArgument != nullptr)
    {
      placeholderArgument->appendValue(output, segment.spec);
    }
  }
  m_output.value(output);
  return &m_output;
}

Core* DataConverterStringTemplate::clone() const
{
  auto cloned = DataConverterStringTemplateBase::clone()
                    ->as<DataConverterStringTemplate>();
  // DataConverter::copy cloned the binds in order and targeted them at the
  // clone, retarget the ones that belong to an argument.
  for (auto argument : m_arguments)
  {
    auto clonedArgument = argument->clone()->as<StringTemplateArgument>();
    cloned->addArgument(clonedArgument);
    size_t index = 0;
    for (auto dataBind : dataBinds())
    {
      if (dataBind->target() == argument)
      {
        cloned->dataBinds()[index]->target(clonedArgument);
      }
      index++;
    }
  }
  return cloned;
}
