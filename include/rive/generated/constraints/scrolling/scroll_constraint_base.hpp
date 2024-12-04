#ifndef _RIVE_SCROLL_CONSTRAINT_BASE_HPP_
#define _RIVE_SCROLL_CONSTRAINT_BASE_HPP_
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ScrollConstraintBase : public DraggableConstraint
{
protected:
    typedef DraggableConstraint Super;

public:
    static const uint16_t typeKey = 521;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScrollConstraintBase::typeKey:
            case DraggableConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t snapPropertyKey = 724;
    static const uint16_t physicsTypeValuePropertyKey = 727;
    static const uint16_t physicsIdPropertyKey = 726;

protected:
    bool m_Snap = false;
    uint32_t m_PhysicsTypeValue = 0;
    uint32_t m_PhysicsId = -1;

public:
    inline bool snap() const { return m_Snap; }
    void snap(bool value)
    {
        if (m_Snap == value)
        {
            return;
        }
        m_Snap = value;
        snapChanged();
    }

    inline uint32_t physicsTypeValue() const { return m_PhysicsTypeValue; }
    void physicsTypeValue(uint32_t value)
    {
        if (m_PhysicsTypeValue == value)
        {
            return;
        }
        m_PhysicsTypeValue = value;
        physicsTypeValueChanged();
    }

    inline uint32_t physicsId() const { return m_PhysicsId; }
    void physicsId(uint32_t value)
    {
        if (m_PhysicsId == value)
        {
            return;
        }
        m_PhysicsId = value;
        physicsIdChanged();
    }

    Core* clone() const override;
    void copy(const ScrollConstraintBase& object)
    {
        m_Snap = object.m_Snap;
        m_PhysicsTypeValue = object.m_PhysicsTypeValue;
        m_PhysicsId = object.m_PhysicsId;
        DraggableConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case snapPropertyKey:
                m_Snap = CoreBoolType::deserialize(reader);
                return true;
            case physicsTypeValuePropertyKey:
                m_PhysicsTypeValue = CoreUintType::deserialize(reader);
                return true;
            case physicsIdPropertyKey:
                m_PhysicsId = CoreUintType::deserialize(reader);
                return true;
        }
        return DraggableConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void snapChanged() {}
    virtual void physicsTypeValueChanged() {}
    virtual void physicsIdChanged() {}
};
} // namespace rive

#endif