#ifndef _RIVE_DATA_BIND_BASE_HPP_
#define _RIVE_DATA_BIND_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class DataBindBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 446;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataBindBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyKeyPropertyKey = 586;
    static const uint16_t flagsPropertyKey = 587;
    static const uint16_t converterIdPropertyKey = 660;

private:
    uint32_t m_PropertyKey = Core::invalidPropertyKey;
    uint32_t m_Flags = 0;
    uint32_t m_ConverterId = -1;

public:
    inline uint32_t propertyKey() const { return m_PropertyKey; }
    void propertyKey(uint32_t value)
    {
        if (m_PropertyKey == value)
        {
            return;
        }
        m_PropertyKey = value;
        propertyKeyChanged();
    }

    inline uint32_t flags() const { return m_Flags; }
    void flags(uint32_t value)
    {
        if (m_Flags == value)
        {
            return;
        }
        m_Flags = value;
        flagsChanged();
    }

    inline uint32_t converterId() const { return m_ConverterId; }
    void converterId(uint32_t value)
    {
        if (m_ConverterId == value)
        {
            return;
        }
        m_ConverterId = value;
        converterIdChanged();
    }

    Core* clone() const override;
    void copy(const DataBindBase& object)
    {
        m_PropertyKey = object.m_PropertyKey;
        m_Flags = object.m_Flags;
        m_ConverterId = object.m_ConverterId;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyKeyPropertyKey:
                m_PropertyKey = CoreUintType::deserialize(reader);
                return true;
            case flagsPropertyKey:
                m_Flags = CoreUintType::deserialize(reader);
                return true;
            case converterIdPropertyKey:
                m_ConverterId = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void propertyKeyChanged() {}
    virtual void flagsChanged() {}
    virtual void converterIdChanged() {}
};
} // namespace rive

#endif