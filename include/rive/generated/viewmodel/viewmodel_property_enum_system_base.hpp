#ifndef _RIVE_VIEW_MODEL_PROPERTY_ENUM_SYSTEM_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_ENUM_SYSTEM_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
namespace rive
{
class ViewModelPropertyEnumSystemBase : public ViewModelPropertyEnum
{
protected:
    typedef ViewModelPropertyEnum Super;

public:
    static const uint16_t typeKey = 511;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelPropertyEnumSystemBase::typeKey:
            case ViewModelPropertyEnumBase::typeKey:
            case ViewModelPropertyBase::typeKey:
            case ViewModelComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t enumTypePropertyKey = 708;

protected:
    uint32_t m_EnumType = 0;

public:
    inline uint32_t enumType() const { return m_EnumType; }
    void enumType(uint32_t value)
    {
        if (m_EnumType == value)
        {
            return;
        }
        m_EnumType = value;
        enumTypeChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelPropertyEnumSystemBase& object)
    {
        m_EnumType = object.m_EnumType;
        ViewModelPropertyEnum::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case enumTypePropertyKey:
                m_EnumType = CoreUintType::deserialize(reader);
                return true;
        }
        return ViewModelPropertyEnum::deserialize(propertyKey, reader);
    }

protected:
    virtual void enumTypeChanged() {}
};
} // namespace rive

#endif