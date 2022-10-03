#ifndef _RIVE_TRANSFORM_SPACE_CONSTRAINT_BASE_HPP_
#define _RIVE_TRANSFORM_SPACE_CONSTRAINT_BASE_HPP_
#include "rive/constraints/targeted_constraint.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TransformSpaceConstraintBase : public TargetedConstraint
{
protected:
    typedef TargetedConstraint Super;

public:
    static const uint16_t typeKey = 90;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransformSpaceConstraintBase::typeKey:
            case TargetedConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t sourceSpaceValuePropertyKey = 179;
    static const uint16_t destSpaceValuePropertyKey = 180;

private:
    uint32_t m_SourceSpaceValue = 0;
    uint32_t m_DestSpaceValue = 0;

public:
    inline uint32_t sourceSpaceValue() const { return m_SourceSpaceValue; }
    void sourceSpaceValue(uint32_t value)
    {
        if (m_SourceSpaceValue == value)
        {
            return;
        }
        m_SourceSpaceValue = value;
        sourceSpaceValueChanged();
    }

    inline uint32_t destSpaceValue() const { return m_DestSpaceValue; }
    void destSpaceValue(uint32_t value)
    {
        if (m_DestSpaceValue == value)
        {
            return;
        }
        m_DestSpaceValue = value;
        destSpaceValueChanged();
    }

    void copy(const TransformSpaceConstraintBase& object)
    {
        m_SourceSpaceValue = object.m_SourceSpaceValue;
        m_DestSpaceValue = object.m_DestSpaceValue;
        TargetedConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case sourceSpaceValuePropertyKey:
                m_SourceSpaceValue = CoreUintType::deserialize(reader);
                return true;
            case destSpaceValuePropertyKey:
                m_DestSpaceValue = CoreUintType::deserialize(reader);
                return true;
        }
        return TargetedConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void sourceSpaceValueChanged() {}
    virtual void destSpaceValueChanged() {}
};
} // namespace rive

#endif