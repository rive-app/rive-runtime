#ifndef _RIVE_SCRIPT_INPUT_ARTBOARD_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_ARTBOARD_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/custom_property.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
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
    void copy(const ScriptInputArtboardBase& object)
    {
        m_ArtboardId = object.m_ArtboardId;
        RIVE_EDITOR_COPY(object);
        CustomProperty::copy(object);
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
        return CustomProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void artboardIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/script_input_artboard_ext.inl"
#endif
};
} // namespace rive

#endif