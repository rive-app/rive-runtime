#ifndef _RIVE_DATA_CONVERTER_STRING_TEMPLATE_BASE_HPP_
#define _RIVE_DATA_CONVERTER_STRING_TEMPLATE_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterStringTemplateBase : public DataConverter
{
protected:
  typedef DataConverter Super;

public:
  static const uint16_t typeKey = 177;

  uint16_t coreType() const override { return typeKey; }

  static const uint16_t openDelimiterPropertyKey = 485;
  static const uint16_t closeDelimiterPropertyKey = 486;

protected:
  std::string m_OpenDelimiter = "\x24{";
  std::string m_CloseDelimiter = "}";

public:
  inline const std::string& openDelimiter() const { return m_OpenDelimiter; }
  void openDelimiter(std::string value)
  {
    if (m_OpenDelimiter == value)
    {
      return;
    }
    RIVE_EDITOR_STRING_CHANGING(
        openDelimiterPropertyKey,
        m_OpenDelimiter,
        value);
    m_OpenDelimiter = value;
    RIVE_EDITOR_CHANGED(openDelimiterChanged());
    notifyPropertyChanged(openDelimiterPropertyKey);
  }

  inline const std::string& closeDelimiter() const { return m_CloseDelimiter; }
  void closeDelimiter(std::string value)
  {
    if (m_CloseDelimiter == value)
    {
      return;
    }
    RIVE_EDITOR_STRING_CHANGING(
        closeDelimiterPropertyKey,
        m_CloseDelimiter,
        value);
    m_CloseDelimiter = value;
    RIVE_EDITOR_CHANGED(closeDelimiterChanged());
    notifyPropertyChanged(closeDelimiterPropertyKey);
  }

  Core* clone() const override;
  void copy(const DataConverterStringTemplateBase& object)
  {
    m_OpenDelimiter = object.m_OpenDelimiter;
    m_CloseDelimiter = object.m_CloseDelimiter;
    DataConverter::copy(object);
  }

  bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
  {
    switch (propertyKey)
    {
      case openDelimiterPropertyKey:
        m_OpenDelimiter = CoreStringType::deserialize(reader);
        return true;
      case closeDelimiterPropertyKey:
        m_CloseDelimiter = CoreStringType::deserialize(reader);
        return true;
    }
    return DataConverter::deserialize(propertyKey, reader);
  }

protected:
  virtual void openDelimiterChanged() {}
  virtual void closeDelimiterChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/data_converter_string_template_ext.inl"
#endif
};
} // namespace rive

#endif