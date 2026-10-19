#ifndef _RIVE_TEXT_INPUT_BASE_HPP_
#define _RIVE_TEXT_INPUT_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/drawable.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class TextInputBase : public Drawable
{
protected:
  typedef Drawable Super;

public:
  static const uint16_t typeKey = 569;

  uint16_t coreType() const override { return typeKey; }

  static const uint16_t textPropertyKey = 817;
  static const uint16_t selectionRadiusPropertyKey = 818;
  static const uint16_t multilinePropertyKey = 979;
  static const uint16_t alignValuePropertyKey = 222;
  static const uint16_t verticalAlignValuePropertyKey = 1094;
  static const uint16_t obscuredPropertyKey = 1095;
  static const uint16_t selectAllOnFocusPropertyKey = 1099;
  static const uint16_t maxCharactersPropertyKey = 489;
  static const uint16_t maxLinesPropertyKey = 490;

protected:
  std::string m_Text = "";
  float m_SelectionRadius = 5.0f;
  bool m_Multiline = true;
  uint32_t m_AlignValue = 0;
  uint32_t m_VerticalAlignValue = 0;
  bool m_Obscured = false;
  bool m_SelectAllOnFocus = false;
  uint32_t m_MaxCharacters = 0;
  uint32_t m_MaxLines = 0;

public:
  inline const std::string& text() const { return m_Text; }
  void text(std::string value)
  {
    if (m_Text == value)
    {
      return;
    }
    RIVE_EDITOR_STRING_CHANGING(textPropertyKey, m_Text, value);
    m_Text = value;
    RIVE_EDITOR_CHANGED(textChanged());
    notifyPropertyChanged(textPropertyKey);
  }

  inline float selectionRadius() const { return m_SelectionRadius; }
  void selectionRadius(float value)
  {
    if (m_SelectionRadius == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(
        selectionRadiusPropertyKey,
        &m_SelectionRadius,
        &value);
    m_SelectionRadius = value;
    RIVE_EDITOR_CHANGED(selectionRadiusChanged());
    notifyPropertyChanged(selectionRadiusPropertyKey);
  }

  inline bool multiline() const { return m_Multiline; }
  void multiline(bool value)
  {
    if (m_Multiline == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(multilinePropertyKey, &m_Multiline, &value);
    m_Multiline = value;
    RIVE_EDITOR_CHANGED(multilineChanged());
    notifyPropertyChanged(multilinePropertyKey);
  }

  inline uint32_t alignValue() const { return m_AlignValue; }
  void alignValue(uint32_t value)
  {
    if (m_AlignValue == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(alignValuePropertyKey, &m_AlignValue, &value);
    m_AlignValue = value;
    RIVE_EDITOR_CHANGED(alignValueChanged());
    notifyPropertyChanged(alignValuePropertyKey);
  }

  inline uint32_t verticalAlignValue() const { return m_VerticalAlignValue; }
  void verticalAlignValue(uint32_t value)
  {
    if (m_VerticalAlignValue == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(
        verticalAlignValuePropertyKey,
        &m_VerticalAlignValue,
        &value);
    m_VerticalAlignValue = value;
    RIVE_EDITOR_CHANGED(verticalAlignValueChanged());
    notifyPropertyChanged(verticalAlignValuePropertyKey);
  }

  inline bool obscured() const { return m_Obscured; }
  void obscured(bool value)
  {
    if (m_Obscured == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(obscuredPropertyKey, &m_Obscured, &value);
    m_Obscured = value;
    RIVE_EDITOR_CHANGED(obscuredChanged());
    notifyPropertyChanged(obscuredPropertyKey);
  }

  inline bool selectAllOnFocus() const { return m_SelectAllOnFocus; }
  void selectAllOnFocus(bool value)
  {
    if (m_SelectAllOnFocus == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(
        selectAllOnFocusPropertyKey,
        &m_SelectAllOnFocus,
        &value);
    m_SelectAllOnFocus = value;
    notifyPropertyChanged(selectAllOnFocusPropertyKey);
  }

  inline uint32_t maxCharacters() const { return m_MaxCharacters; }
  void maxCharacters(uint32_t value)
  {
    if (m_MaxCharacters == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(maxCharactersPropertyKey, &m_MaxCharacters, &value);
    m_MaxCharacters = value;
    RIVE_EDITOR_CHANGED(maxCharactersChanged());
    notifyPropertyChanged(maxCharactersPropertyKey);
  }

  inline uint32_t maxLines() const { return m_MaxLines; }
  void maxLines(uint32_t value)
  {
    if (m_MaxLines == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(maxLinesPropertyKey, &m_MaxLines, &value);
    m_MaxLines = value;
    RIVE_EDITOR_CHANGED(maxLinesChanged());
    notifyPropertyChanged(maxLinesPropertyKey);
  }

  Core* clone() const override;
  void copy(const TextInputBase& object)
  {
    m_Text = object.m_Text;
    m_SelectionRadius = object.m_SelectionRadius;
    m_Multiline = object.m_Multiline;
    m_AlignValue = object.m_AlignValue;
    m_VerticalAlignValue = object.m_VerticalAlignValue;
    m_Obscured = object.m_Obscured;
    m_SelectAllOnFocus = object.m_SelectAllOnFocus;
    m_MaxCharacters = object.m_MaxCharacters;
    m_MaxLines = object.m_MaxLines;
    RIVE_EDITOR_COPY(object);
    Drawable::copy(object);
  }

  bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
  {
    switch (propertyKey)
    {
      case textPropertyKey:
        m_Text = CoreStringType::deserialize(reader);
        return true;
      case selectionRadiusPropertyKey:
        m_SelectionRadius = CoreDoubleType::deserialize(reader);
        return true;
      case multilinePropertyKey:
        m_Multiline = CoreBoolType::deserialize(reader);
        return true;
      case alignValuePropertyKey:
        m_AlignValue = CoreUintType::deserialize(reader);
        return true;
      case verticalAlignValuePropertyKey:
        m_VerticalAlignValue = CoreUintType::deserialize(reader);
        return true;
      case obscuredPropertyKey:
        m_Obscured = CoreBoolType::deserialize(reader);
        return true;
      case selectAllOnFocusPropertyKey:
        m_SelectAllOnFocus = CoreBoolType::deserialize(reader);
        return true;
      case maxCharactersPropertyKey:
        m_MaxCharacters = CoreUintType::deserialize(reader);
        return true;
      case maxLinesPropertyKey:
        m_MaxLines = CoreUintType::deserialize(reader);
        return true;
    }
    RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
    return Drawable::deserialize(propertyKey, reader);
  }

protected:
  virtual void textChanged() {}
  virtual void selectionRadiusChanged() {}
  virtual void multilineChanged() {}
  virtual void alignValueChanged() {}
  virtual void verticalAlignValueChanged() {}
  virtual void obscuredChanged() {}
  virtual void maxCharactersChanged() {}
  virtual void maxLinesChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/text/text_input_ext.inl"
#endif
};
} // namespace rive

#endif