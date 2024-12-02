#ifndef _RIVE_SCROLL_CONSTRAINT_BASE_HPP_
#define _RIVE_SCROLL_CONSTRAINT_BASE_HPP_
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
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

protected:
    bool m_Snap = false;

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

    Core* clone() const override;
    void copy(const ScrollConstraintBase& object)
    {
        m_Snap = object.m_Snap;
        DraggableConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case snapPropertyKey:
                m_Snap = CoreBoolType::deserialize(reader);
                return true;
        }
        return DraggableConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void snapChanged() {}
};
} // namespace rive

#endif