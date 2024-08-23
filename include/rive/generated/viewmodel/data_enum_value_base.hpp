#ifndef _RIVE_DATA_ENUM_VALUE_BASE_HPP_
#define _RIVE_DATA_ENUM_VALUE_BASE_HPP_
#include <string>
#include "rive/core.hpp"
#include "rive/core/field_types/core_string_type.hpp"
namespace rive
{
class DataEnumValueBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 445;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataEnumValueBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t keyPropertyKey = 578;
    static const uint16_t valuePropertyKey = 579;

private:
    std::string m_Key = "";
    std::string m_Value = "";

public:
    inline const std::string& key() const { return m_Key; }
    void key(std::string value)
    {
        if (m_Key == value)
        {
            return;
        }
        m_Key = value;
        keyChanged();
    }

    inline const std::string& value() const { return m_Value; }
    void value(std::string value)
    {
        if (m_Value == value)
        {
            return;
        }
        m_Value = value;
        valueChanged();
    }

    Core* clone() const override;
    void copy(const DataEnumValueBase& object)
    {
        m_Key = object.m_Key;
        m_Value = object.m_Value;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case keyPropertyKey:
                m_Key = CoreStringType::deserialize(reader);
                return true;
            case valuePropertyKey:
                m_Value = CoreStringType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void keyChanged() {}
    virtual void valueChanged() {}
};
} // namespace rive

#endif