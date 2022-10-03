#ifndef _RIVE_KEYED_PROPERTY_BASE_HPP_
#define _RIVE_KEYED_PROPERTY_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class KeyedPropertyBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 26;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyedPropertyBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyKeyPropertyKey = 53;

private:
    uint32_t m_PropertyKey = Core::invalidPropertyKey;

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

    Core* clone() const override;
    void copy(const KeyedPropertyBase& object) { m_PropertyKey = object.m_PropertyKey; }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyKeyPropertyKey:
                m_PropertyKey = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void propertyKeyChanged() {}
};
} // namespace rive

#endif