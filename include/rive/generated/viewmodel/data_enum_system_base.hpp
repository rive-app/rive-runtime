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
        RIVE_EDITOR_CHANGING(enumTypePropertyKey, &m_EnumType, &value);
        m_EnumType = value;
        notifyPropertyChanged(enumTypePropertyKey);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/data_enum_system_ext.inl"
#endif
};
} // namespace rive

#endif