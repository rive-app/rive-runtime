#ifndef _RIVE_BACKBOARD_BASE_HPP_
#define _RIVE_BACKBOARD_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class BackboardBase : public Core
{
protected:
  typedef Core Super;

public:
  static const uint16_t typeKey = 23;

  uint16_t coreType() const override { return typeKey; }

  static const uint16_t runtimeFlagsPropertyKey = 1148;

protected:
  uint8_t m_RuntimeFlags = 1;

public:
  inline uint8_t runtimeFlags() const { return m_RuntimeFlags; }
  void runtimeFlags(uint8_t value)
  {
    if (m_RuntimeFlags == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(runtimeFlagsPropertyKey, &m_RuntimeFlags, &value);
    m_RuntimeFlags = value;
    notifyPropertyChanged(runtimeFlagsPropertyKey);
  }

  Core* clone() const override;
  void copy(const BackboardBase& object)
  {
    m_RuntimeFlags = object.m_RuntimeFlags;
    RIVE_EDITOR_COPY(object);
    RIVE_EDITOR_COPY_VALIDATED(object);
  }

  bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
  {
    switch (propertyKey)
    {
      case runtimeFlagsPropertyKey:
        m_RuntimeFlags = CoreUintType::deserialize(reader);
        return true;
    }
    RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
    return false;
  }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/backboard_ext.inl"
#endif
};
} // namespace rive

#endif