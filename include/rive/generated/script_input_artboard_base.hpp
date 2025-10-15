#ifndef _RIVE_SCRIPT_INPUT_ARTBOARD_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_ARTBOARD_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/custom_property.hpp"
namespace rive
{
class ScriptInputArtboardBase : public CustomProperty
{
protected:
    typedef CustomProperty Super;

public:
    static const uint16_t typeKey = 621;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptInputArtboardBase::typeKey:
            case CustomPropertyBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t artboardIdPropertyKey = 876;

protected:
    uint32_t m_ArtboardId = -1;

public:
    inline uint32_t artboardId() const { return m_ArtboardId; }
    void artboardId(uint32_t value)
    {
        if (m_ArtboardId == value)
        {
            return;
        }
        m_ArtboardId = value;
        artboardIdChanged();
    }

    Core* clone() const override;
    void copy(const ScriptInputArtboardBase& object)
    {
        m_ArtboardId = object.m_ArtboardId;
        CustomProperty::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case artboardIdPropertyKey:
                m_ArtboardId = CoreUintType::deserialize(reader);
                return true;
        }
        return CustomProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void artboardIdChanged() {}
};
} // namespace rive

#endif