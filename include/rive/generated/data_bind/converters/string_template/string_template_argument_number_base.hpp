#ifndef _RIVE_STRING_TEMPLATE_ARGUMENT_NUMBER_BASE_HPP_
#define _RIVE_STRING_TEMPLATE_ARGUMENT_NUMBER_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/data_bind/converters/string_template/string_template_argument.hpp"
namespace rive
{
class StringTemplateArgumentNumberBase : public StringTemplateArgument
{
protected:
  typedef StringTemplateArgument Super;

public:
  static const uint16_t typeKey = 176;

  uint16_t coreType() const override { return typeKey; }

  static const uint16_t propertyValuePropertyKey = 483;

protected:
  float m_PropertyValue = 0.0f;

public:
  inline float propertyValue() const { return m_PropertyValue; }
  void propertyValue(float value)
  {
    if (m_PropertyValue == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(propertyValuePropertyKey, &m_PropertyValue, &value);
    m_PropertyValue = value;
    RIVE_EDITOR_CHANGED(propertyValueChanged());
    notifyPropertyChanged(propertyValuePropertyKey);
  }

  Core* clone() const override;
  void copy(const StringTemplateArgumentNumberBase& object)
  {
    m_PropertyValue = object.m_PropertyValue;
    StringTemplateArgument::copy(object);
  }

  bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
  {
    switch (propertyKey)
    {
      case propertyValuePropertyKey:
        m_PropertyValue = CoreDoubleType::deserialize(reader);
        return true;
    }
    return StringTemplateArgument::deserialize(propertyKey, reader);
  }

protected:
  virtual void propertyValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/string_template/string_template_argument_number_ext.inl"
#endif
};
} // namespace rive

#endif