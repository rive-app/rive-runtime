#ifndef _RIVE_SCRIPT_INPUT_COMPONENT_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_COMPONENT_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/custom_property.hpp"
namespace rive
{
class ScriptInputComponentBase : public CustomProperty
{
protected:
  typedef CustomProperty Super;

public:
  static const uint16_t typeKey = 199;

  uint16_t coreType() const override { return typeKey; }

  static const uint16_t componentIdPropertyKey = 1138;

protected:
  Id m_ComponentId = kEmptyId;

public:
  inline Id componentId() const { return m_ComponentId; }
  void componentId(Id value)
  {
    if (m_ComponentId == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(componentIdPropertyKey, &m_ComponentId, &value);
    m_ComponentId = value;
    notifyPropertyChanged(componentIdPropertyKey);
  }

  Core* clone() const override;
  void copy(const ScriptInputComponentBase& object)
  {
    m_ComponentId = object.m_ComponentId;
    CustomProperty::copy(object);
  }

  bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
  {
    switch (propertyKey)
    {
      case componentIdPropertyKey:
        m_ComponentId = CoreIdType::runtimeDeserialize(reader);
        return true;
    }
    return CustomProperty::deserialize(propertyKey, reader);
  }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/script_input_component_ext.inl"
#endif
};
} // namespace rive

#endif