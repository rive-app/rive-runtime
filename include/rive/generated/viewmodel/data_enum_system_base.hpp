#ifndef _RIVE_DATA_ENUM_SYSTEM_BASE_HPP_
#define _RIVE_DATA_ENUM_SYSTEM_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/data_enum.hpp"
namespace rive
{
class DataEnumSystemBase : public DataEnum
{
protected:
    typedef DataEnum Super;

public:
    static const uint16_t typeKey = 512;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataEnumSystemBase::typeKey:
            case DataEnumBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t enumTypePropertyKey = 709;

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
    void copy(const DataEnumSystemBase& object)
    {
        m_EnumType = object.m_EnumType;
        DataEnum::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case enumTypePropertyKey:
                m_EnumType = CoreUintType::deserialize(reader);
                return true;
        }
        return DataEnum::deserialize(propertyKey, reader);
    }

protected:
    virtual void enumTypeChanged() {}
};
} // namespace rive

#endif