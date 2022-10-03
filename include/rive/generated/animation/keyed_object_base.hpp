#ifndef _RIVE_KEYED_OBJECT_BASE_HPP_
#define _RIVE_KEYED_OBJECT_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class KeyedObjectBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 25;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyedObjectBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t objectIdPropertyKey = 51;

private:
    uint32_t m_ObjectId = 0;

public:
    inline uint32_t objectId() const { return m_ObjectId; }
    void objectId(uint32_t value)
    {
        if (m_ObjectId == value)
        {
            return;
        }
        m_ObjectId = value;
        objectIdChanged();
    }

    Core* clone() const override;
    void copy(const KeyedObjectBase& object) { m_ObjectId = object.m_ObjectId; }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case objectIdPropertyKey:
                m_ObjectId = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void objectIdChanged() {}
};
} // namespace rive

#endif