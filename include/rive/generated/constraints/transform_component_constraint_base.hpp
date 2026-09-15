#ifndef _RIVE_TRANSFORM_COMPONENT_CONSTRAINT_BASE_HPP_
#define _RIVE_TRANSFORM_COMPONENT_CONSTRAINT_BASE_HPP_
#include "rive/constraints/transform_space_constraint.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TransformComponentConstraintBase : public TransformSpaceConstraint
{
protected:
    typedef TransformSpaceConstraint Super;

public:
    static const uint16_t typeKey = 85;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransformComponentConstraintBase::typeKey:
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

    static const uint16_t minMaxSpaceValuePropertyKey = 195;
    static const uint16_t copyFactorPropertyKey = 182;
    static const uint16_t minValuePropertyKey = 183;
    static const uint16_t maxValuePropertyKey = 184;
    static const uint16_t offsetPropertyKey = 188;
    static const uint16_t doesCopyPropertyKey = 189;
    static const uint16_t minPropertyKey = 190;
    static const uint16_t maxPropertyKey = 191;

protected:
    uint32_t m_MinMaxSpaceValue = 0;
    float m_CopyFactor = 1.0f;
    float m_MinValue = 0.0f;
    float m_MaxValue = 0.0f;
    bool m_Offset = false;
    bool m_DoesCopy = true;
    bool m_Min = false;
    bool m_Max = false;

public:
    inline uint32_t minMaxSpaceValue() const { return m_MinMaxSpaceValue; }
    void minMaxSpaceValue(uint32_t value)
    {
        if (m_MinMaxSpaceValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minMaxSpaceValuePropertyKey,
                             &m_MinMaxSpaceValue,
                             &value);
        m_MinMaxSpaceValue = value;
        RIVE_EDITOR_CHANGED(minMaxSpaceValueChanged());
        notifyPropertyChanged(minMaxSpaceValuePropertyKey);
    }

    inline float copyFactor() const { return m_CopyFactor; }
    void copyFactor(float value)
    {
        if (m_CopyFactor == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(copyFactorPropertyKey, &m_CopyFactor, &value);
        m_CopyFactor = value;
        RIVE_EDITOR_CHANGED(copyFactorChanged());
        notifyPropertyChanged(copyFactorPropertyKey);
    }

    inline float minValue() const { return m_MinValue; }
    void minValue(float value)
    {
        if (m_MinValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minValuePropertyKey, &m_MinValue, &value);
        m_MinValue = value;
        RIVE_EDITOR_CHANGED(minValueChanged());
        notifyPropertyChanged(minValuePropertyKey);
    }

    inline float maxValue() const { return m_MaxValue; }
    void maxValue(float value)
    {
        if (m_MaxValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maxValuePropertyKey, &m_MaxValue, &value);
        m_MaxValue = value;
        RIVE_EDITOR_CHANGED(maxValueChanged());
        notifyPropertyChanged(maxValuePropertyKey);
    }

    inline bool offset() const { return m_Offset; }
    void offset(bool value)
    {
        if (m_Offset == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(offsetPropertyKey, &m_Offset, &value);
        m_Offset = value;
        RIVE_EDITOR_CHANGED(offsetChanged());
        notifyPropertyChanged(offsetPropertyKey);
    }

    inline bool doesCopy() const { return m_DoesCopy; }
    void doesCopy(bool value)
    {
        if (m_DoesCopy == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(doesCopyPropertyKey, &m_DoesCopy, &value);
        m_DoesCopy = value;
        RIVE_EDITOR_CHANGED(doesCopyChanged());
        notifyPropertyChanged(doesCopyPropertyKey);
    }

    inline bool min() const { return m_Min; }
    void min(bool value)
    {
        if (m_Min == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minPropertyKey, &m_Min, &value);
        m_Min = value;
        RIVE_EDITOR_CHANGED(minChanged());
        notifyPropertyChanged(minPropertyKey);
    }

    inline bool max() const { return m_Max; }
    void max(bool value)
    {
        if (m_Max == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maxPropertyKey, &m_Max, &value);
        m_Max = value;
        RIVE_EDITOR_CHANGED(maxChanged());
        notifyPropertyChanged(maxPropertyKey);
    }

    void copy(const TransformComponentConstraintBase& object)
    {
        m_MinMaxSpaceValue = object.m_MinMaxSpaceValue;
        m_CopyFactor = object.m_CopyFactor;
        m_MinValue = object.m_MinValue;
        m_MaxValue = object.m_MaxValue;
        m_Offset = object.m_Offset;
        m_DoesCopy = object.m_DoesCopy;
        m_Min = object.m_Min;
        m_Max = object.m_Max;
        TransformSpaceConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case minMaxSpaceValuePropertyKey:
                m_MinMaxSpaceValue = CoreUintType::deserialize(reader);
                return true;
            case copyFactorPropertyKey:
                m_CopyFactor = CoreDoubleType::deserialize(reader);
                return true;
            case minValuePropertyKey:
                m_MinValue = CoreDoubleType::deserialize(reader);
                return true;
            case maxValuePropertyKey:
                m_MaxValue = CoreDoubleType::deserialize(reader);
                return true;
            case offsetPropertyKey:
                m_Offset = CoreBoolType::deserialize(reader);
                return true;
            case doesCopyPropertyKey:
                m_DoesCopy = CoreBoolType::deserialize(reader);
                return true;
            case minPropertyKey:
                m_Min = CoreBoolType::deserialize(reader);
                return true;
            case maxPropertyKey:
                m_Max = CoreBoolType::deserialize(reader);
                return true;
        }
        return TransformSpaceConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void minMaxSpaceValueChanged() {}
    virtual void copyFactorChanged() {}
    virtual void minValueChanged() {}
    virtual void maxValueChanged() {}
    virtual void offsetChanged() {}
    virtual void doesCopyChanged() {}
    virtual void minChanged() {}
    virtual void maxChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/constraints/transform_component_constraint_ext.inl"
#endif
};
} // namespace rive

#endif