#ifndef _RIVE_TARGETED_CONSTRAINT_BASE_HPP_
#define _RIVE_TARGETED_CONSTRAINT_BASE_HPP_
#include "rive/constraints/constraint.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class TargetedConstraintBase : public Constraint
{
protected:
    typedef Constraint Super;

public:
    static const uint16_t typeKey = 80;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

protected:
    Id m_TargetId = kEmptyId;

public:
    inline Id targetId() const { return m_TargetId; }
    void targetId(Id value)
    {
        if (m_TargetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(targetIdPropertyKey, &m_TargetId, &value);
        m_TargetId = value;
        RIVE_EDITOR_CHANGED(targetIdChanged());
        notifyPropertyChanged(targetIdPropertyKey);
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
                m_TargetId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return Constraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/constraints/targeted_constraint_ext.inl"
#endif
};
} // namespace rive

#endif