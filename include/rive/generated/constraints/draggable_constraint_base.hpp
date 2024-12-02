#ifndef _RIVE_DRAGGABLE_CONSTRAINT_BASE_HPP_
#define _RIVE_DRAGGABLE_CONSTRAINT_BASE_HPP_
#include "rive/constraints/constraint.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class DraggableConstraintBase : public Constraint
{
protected:
    typedef Constraint Super;

public:
    static const uint16_t typeKey = 520;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DraggableConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t directionValuePropertyKey = 722;

protected:
    uint32_t m_DirectionValue = 1;

public:
    inline uint32_t directionValue() const { return m_DirectionValue; }
    void directionValue(uint32_t value)
    {
        if (m_DirectionValue == value)
        {
            return;
        }
        m_DirectionValue = value;
        directionValueChanged();
    }

    void copy(const DraggableConstraintBase& object)
    {
        m_DirectionValue = object.m_DirectionValue;
        Constraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case directionValuePropertyKey:
                m_DirectionValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Constraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void directionValueChanged() {}
};
} // namespace rive

#endif