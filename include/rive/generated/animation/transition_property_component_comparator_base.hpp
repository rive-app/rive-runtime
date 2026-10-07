#ifndef _RIVE_TRANSITION_PROPERTY_COMPONENT_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_PROPERTY_COMPONENT_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_property_comparator.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class TransitionPropertyComponentComparatorBase
    : public TransitionPropertyComparator
{
protected:
    typedef TransitionPropertyComparator Super;

public:
    static const uint16_t typeKey = 667;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t objectIdPropertyKey = 977;
    static const uint16_t propertyKeyPropertyKey = 978;

protected:
    Id m_ObjectId = 0;
    uint32_t m_PropertyKey = 0;

public:
    inline Id objectId() const { return m_ObjectId; }
    void objectId(Id value)
    {
        if (m_ObjectId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(objectIdPropertyKey, &m_ObjectId, &value);
        m_ObjectId = value;
        notifyPropertyChanged(objectIdPropertyKey);
    }

    inline uint32_t propertyKey() const { return m_PropertyKey; }
    void propertyKey(uint32_t value)
    {
        if (m_PropertyKey == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(propertyKeyPropertyKey, &m_PropertyKey, &value);
        m_PropertyKey = value;
        notifyPropertyChanged(propertyKeyPropertyKey);
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
                m_ObjectId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case propertyKeyPropertyKey:
                m_PropertyKey = CoreUintType::deserialize(reader);
                return true;
        }
        return TransitionPropertyComparator::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/transition_property_component_comparator_ext.inl"
#endif
};
} // namespace rive

#endif