#ifndef _RIVE_TARGETED_CONSTRAINT_BASE_HPP_
#define _RIVE_TARGETED_CONSTRAINT_BASE_HPP_
#include "rive/constraints/constraint.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TargetedConstraintBase : public Constraint
{
protected:
    typedef Constraint Super;

public:
    static const uint16_t typeKey = 80;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TargetedConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 173;

private:
    uint32_t m_TargetId = -1;

public:
    inline uint32_t targetId() const { return m_TargetId; }
    void targetId(uint32_t value)
    {
        if (m_TargetId == value)
        {
            return;
        }
        m_TargetId = value;
        targetIdChanged();
    }

    void copy(const TargetedConstraintBase& object)
    {
        m_TargetId = object.m_TargetId;
        Constraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreUintType::deserialize(reader);
                return true;
        }
        return Constraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
};
} // namespace rive

#endif