#ifndef _RIVE_TRANSITION_PROPERTY_COMPONENT_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_PROPERTY_COMPONENT_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_property_comparator.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TransitionPropertyComponentComparatorBase
    : public TransitionPropertyComparator
{
protected:
    typedef TransitionPropertyComparator Super;

public:
    static const uint16_t typeKey = 667;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionPropertyComponentComparatorBase::typeKey:
            case TransitionPropertyComparatorBase::typeKey:
            case TransitionComparatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t objectIdPropertyKey = 977;
    static const uint16_t propertyKeyPropertyKey = 978;

protected:
    uint32_t m_ObjectId = 0;
    uint32_t m_PropertyKey = 0;

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
    void copy(const TransitionPropertyComponentComparatorBase& object)
    {
        m_ObjectId = object.m_ObjectId;
        m_PropertyKey = object.m_PropertyKey;
        TransitionPropertyComparator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case objectIdPropertyKey:
                m_ObjectId = CoreUintType::deserialize(reader);
                return true;
            case propertyKeyPropertyKey:
                m_PropertyKey = CoreUintType::deserialize(reader);
                return true;
        }
        return TransitionPropertyComparator::deserialize(propertyKey, reader);
    }

protected:
    virtual void objectIdChanged() {}
    virtual void propertyKeyChanged() {}
};
} // namespace rive

#endif