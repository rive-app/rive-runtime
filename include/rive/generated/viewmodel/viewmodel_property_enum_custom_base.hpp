#ifndef _RIVE_VIEW_MODEL_PROPERTY_ENUM_CUSTOM_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_ENUM_CUSTOM_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
namespace rive
{
class ViewModelPropertyEnumCustomBase : public ViewModelPropertyEnum
{
protected:
    typedef ViewModelPropertyEnum Super;

public:
    static const uint16_t typeKey = 439;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelPropertyEnumCustomBase::typeKey:
            case ViewModelPropertyEnumBase::typeKey:
            case ViewModelPropertyBase::typeKey:
            case ViewModelComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t enumIdPropertyKey = 574;

protected:
    Id m_EnumId = kEmptyId;

public:
    inline Id enumId() const { return m_EnumId; }
    void enumId(Id value)
    {
        if (m_EnumId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(enumIdPropertyKey, &m_EnumId, &value);
        m_EnumId = value;
        RIVE_EDITOR_CHANGED(enumIdChanged());
        notifyPropertyChanged(enumIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelPropertyEnumCustomBase& object)
    {
        m_EnumId = object.m_EnumId;
        ViewModelPropertyEnum::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case enumIdPropertyKey:
                m_EnumId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return ViewModelPropertyEnum::deserialize(propertyKey, reader);
    }

protected:
    virtual void enumIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_property_enum_custom_ext.inl"
#endif
};
} // namespace rive

#endif