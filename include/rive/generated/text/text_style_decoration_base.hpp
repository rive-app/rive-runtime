#ifndef _RIVE_TEXT_STYLE_DECORATION_BASE_HPP_
#define _RIVE_TEXT_STYLE_DECORATION_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TextStyleDecorationBase : public ContainerComponent
{
protected:
  typedef ContainerComponent Super;

public:
  static const uint16_t typeKey = 207;

  uint16_t coreType() const override { return typeKey; }

  static const uint16_t decorationTypeValuePropertyKey = 1149;
  static const uint16_t thicknessPropertyKey = 1150;
  static const uint16_t offsetPropertyKey = 1151;

protected:
  uint8_t m_DecorationTypeValue = 0;
  float m_Thickness = -1.0f;
  float m_Offset = 0.0f;

public:
  inline uint8_t decorationTypeValue() const { return m_DecorationTypeValue; }
  void decorationTypeValue(uint8_t value)
  {
    if (m_DecorationTypeValue == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(
        decorationTypeValuePropertyKey,
        &m_DecorationTypeValue,
        &value);
    m_DecorationTypeValue = value;
    RIVE_EDITOR_CHANGED(decorationTypeValueChanged());
    notifyPropertyChanged(decorationTypeValuePropertyKey);
  }

  inline float thickness() const { return m_Thickness; }
  void thickness(float value)
  {
    if (m_Thickness == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(thicknessPropertyKey, &m_Thickness, &value);
    m_Thickness = value;
    RIVE_EDITOR_CHANGED(thicknessChanged());
    notifyPropertyChanged(thicknessPropertyKey);
  }

  inline float offset() const { return m_Offset; }
  void offset(float value)
  {
    if (m_Offset == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(offsetPropertyKey, &m_Offset, &value);
    m_Offset = value;
    RIVE_EDITOR_CHANGED(offsetChanged());
    notifyPropertyChanged(offsetPropertyKey);
  }

  Core* clone() const override;
  void copy(const TextStyleDecorationBase& object)
  {
    m_DecorationTypeValue = object.m_DecorationTypeValue;
    m_Thickness = object.m_Thickness;
    m_Offset = object.m_Offset;
    ContainerComponent::copy(object);
  }

  bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
  {
    switch (propertyKey)
    {
      case decorationTypeValuePropertyKey:
        m_DecorationTypeValue = CoreUintType::deserialize(reader);
        return true;
      case thicknessPropertyKey:
        m_Thickness = CoreDoubleType::deserialize(reader);
        return true;
      case offsetPropertyKey:
        m_Offset = CoreDoubleType::deserialize(reader);
        return true;
    }
    return ContainerComponent::deserialize(propertyKey, reader);
  }

protected:
  virtual void decorationTypeValueChanged() {}
  virtual void thicknessChanged() {}
  virtual void offsetChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/text/text_style_decoration_ext.inl"
#endif
};
} // namespace rive

#endif