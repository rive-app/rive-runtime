#include "rive/data_bind/converters/string_template/string_template_argument.hpp"
#include "rive/data_bind/converters/data_converter_string_template.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/backboard.hpp"

using namespace rive;

StatusCode StringTemplateArgument::import(ImportStack& importStack)
{
  auto backboardImporter = importStack.latest<BackboardImporter>(
      Backboard::typeKey);
  if (backboardImporter == nullptr)
  {
    return StatusCode::MissingObject;
  }
  // Arguments are written right after their template (and its binds), so
  // the template is the last converter imported.
  auto converter = backboardImporter->lastDataConverter();
  if (converter == nullptr || !converter->is<DataConverterStringTemplate>())
  {
    return StatusCode::MissingObject;
  }
  auto code = Super::import(importStack);
  if (code != StatusCode::Ok)
  {
    return code;
  }
  // Only register once nothing can fail: on failure the file deletes this
  // argument, which would leave the template holding a dangling pointer.
  converter->as<DataConverterStringTemplate>()->addArgument(this);
  return StatusCode::Ok;
}

void StringTemplateArgument::addDataBind(DataBind* dataBind)
{
  if (m_stringTemplate)
  {
    m_stringTemplate->addDataBind(dataBind);
  }
}
