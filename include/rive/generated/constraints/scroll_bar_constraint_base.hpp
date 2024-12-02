#ifndef _RIVE_SCROLL_BAR_CONSTRAINT_BASE_HPP_
#define _RIVE_SCROLL_BAR_CONSTRAINT_BASE_HPP_
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ScrollBarConstraintBase : public DraggableConstraint
{
protected:
    typedef DraggableConstraint Super;

public:
    static const uint16_t typeKey = 522;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScrollBarConstraintBase::typeKey:
            case DraggableConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t scrollConstraintIdPropertyKey = 725;

protected:
    uint32_t m_ScrollConstraintId = -1;

public:
    inline uint32_t scrollConstraintId() const { return m_ScrollConstraintId; }
    void scrollConstraintId(uint32_t value)
    {
        if (m_ScrollConstraintId == value)
        {
            return;
        }
        m_ScrollConstraintId = value;
        scrollConstraintIdChanged();
    }

    Core* clone() const override;
    void copy(const ScrollBarConstraintBase& object)
    {
        m_ScrollConstraintId = object.m_ScrollConstraintId;
        DraggableConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case scrollConstraintIdPropertyKey:
                m_ScrollConstraintId = CoreUintType::deserialize(reader);
                return true;
        }
        return DraggableConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void scrollConstraintIdChanged() {}
};
} // namespace rive

#endif