#ifndef _RIVE_DATA_ENUM_CUSTOM_BASE_HPP_
#define _RIVE_DATA_ENUM_CUSTOM_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/viewmodel/data_enum.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class DataEnumCustomBase : public DataEnum
{
protected:
    typedef DataEnum Super;

public:
    static const uint16_t typeKey = 438;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataEnumCustomBase::typeKey:
            case DataEnumBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t namePropertyKey = 572;

protected:
    std::string m_Name = "";

public:
    inline const std::string& name() const { return m_Name; }
    void name(std::string value)
    {
        if (m_Name == value)
        {
            return;
        }
        RIVE_EDITOR_STRING_CHANGING(namePropertyKey, m_Name, value);
        m_Name = value;
        RIVE_EDITOR_CHANGED(nameChanged());
        notifyPropertyChanged(namePropertyKey);
    }

    Core* clone() const override;
    void copy(const DataEnumCustomBase& object)
    {
        m_Name = object.m_Name;
        RIVE_EDITOR_COPY(object);
        DataEnum::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case namePropertyKey:
                m_Name = CoreStringType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return DataEnum::deserialize(propertyKey, reader);
    }

protected:
    virtual void nameChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/data_enum_custom_ext.inl"
#endif
};
} // namespace rive

#endif