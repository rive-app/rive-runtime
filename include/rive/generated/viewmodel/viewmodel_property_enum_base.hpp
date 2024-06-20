#ifndef _RIVE_VIEW_MODEL_PROPERTY_ENUM_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_ENUM_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
namespace rive
{
class ViewModelPropertyEnumBase : public ViewModelProperty
{
protected:
    typedef ViewModelProperty Super;

public:
    static const uint16_t typeKey = 439;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
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

private:
    uint32_t m_EnumId = -1;

public:
    inline uint32_t enumId() const { return m_EnumId; }
    void enumId(uint32_t value)
    {
        if (m_EnumId == value)
        {
            return;
        }
        m_EnumId = value;
        enumIdChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelPropertyEnumBase& object)
    {
        m_EnumId = object.m_EnumId;
        ViewModelProperty::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case enumIdPropertyKey:
                m_EnumId = CoreUintType::deserialize(reader);
                return true;
        }
        return ViewModelProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void enumIdChanged() {}
};
} // namespace rive

#endif