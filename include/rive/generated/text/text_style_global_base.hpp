#ifndef _RIVE_TEXT_STYLE_GLOBAL_BASE_HPP_
#define _RIVE_TEXT_STYLE_GLOBAL_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/text/text_style_reference.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class TextStyleGlobalBase : public TextStyleReference
{
protected:
  typedef TextStyleReference Super;

public:
  static const uint16_t typeKey = 1101;

  uint16_t coreType() const override { return typeKey; }

  static const uint16_t artboardIdPropertyKey = 493;

protected:
  Id m_ArtboardId = kEmptyId;

public:
  inline Id artboardId() const { return m_ArtboardId; }
  void artboardId(Id value)
  {
    if (m_ArtboardId == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(artboardIdPropertyKey, &m_ArtboardId, &value);
    m_ArtboardId = value;
    RIVE_EDITOR_CHANGED(artboardIdChanged());
    notifyPropertyChanged(artboardIdPropertyKey);
  }

  Core* clone() const override;
  void copy(const TextStyleGlobalBase& object)
  {
    m_ArtboardId = object.m_ArtboardId;
    RIVE_EDITOR_COPY(object);
    TextStyleReference::copy(object);
  }

  bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
  {
    switch (propertyKey)
    {
      case artboardIdPropertyKey:
        m_ArtboardId = CoreIdType::runtimeDeserialize(reader);
        return true;
    }
    RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
    return TextStyleReference::deserialize(propertyKey, reader);
  }

protected:
  virtual void artboardIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/text/text_style_global_ext.inl"
#endif
};
} // namespace rive

#endif