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

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t sourceSpaceValuePropertyKey = 179;
    static const uint16_t destSpaceValuePropertyKey = 180;

protected:
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
        RIVE_EDITOR_CHANGING(sourceSpaceValuePropertyKey,
                             &m_SourceSpaceValue,
                             &value);
        m_SourceSpaceValue = value;
        notifyPropertyChanged(sourceSpaceValuePropertyKey);
    }

    inline uint32_t destSpaceValue() const { return m_DestSpaceValue; }
    void destSpaceValue(uint32_t value)
    {
        if (m_DestSpaceValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(destSpaceValuePropertyKey,
                             &m_DestSpaceValue,
                             &value);
        m_DestSpaceValue = value;
        notifyPropertyChanged(destSpaceValuePropertyKey);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/constraints/transform_space_constraint_ext.inl"
#endif
};
} // namespace rive

#endif