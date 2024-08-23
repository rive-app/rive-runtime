#ifndef _RIVE_TRANSFORM_COMPONENT_CONSTRAINT_YBASE_HPP_
#define _RIVE_TRANSFORM_COMPONENT_CONSTRAINT_YBASE_HPP_
#include "rive/constraints/transform_component_constraint.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class TransformComponentConstraintYBase : public TransformComponentConstraint
{
protected:
    typedef TransformComponentConstraint Super;

public:
    static const uint16_t typeKey = 86;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransformComponentConstraintYBase::typeKey:
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

    static const uint16_t copyFactorYPropertyKey = 185;
    static const uint16_t minValueYPropertyKey = 186;
    static const uint16_t maxValueYPropertyKey = 187;
    static const uint16_t doesCopyYPropertyKey = 192;
    static const uint16_t minYPropertyKey = 193;
    static const uint16_t maxYPropertyKey = 194;

private:
    float m_CopyFactorY = 1.0f;
    float m_MinValueY = 0.0f;
    float m_MaxValueY = 0.0f;
    bool m_DoesCopyY = true;
    bool m_MinY = false;
    bool m_MaxY = false;

public:
    inline float copyFactorY() const { return m_CopyFactorY; }
    void copyFactorY(float value)
    {
        if (m_CopyFactorY == value)
        {
            return;
        }
        m_CopyFactorY = value;
        copyFactorYChanged();
    }

    inline float minValueY() const { return m_MinValueY; }
    void minValueY(float value)
    {
        if (m_MinValueY == value)
        {
            return;
        }
        m_MinValueY = value;
        minValueYChanged();
    }

    inline float maxValueY() const { return m_MaxValueY; }
    void maxValueY(float value)
    {
        if (m_MaxValueY == value)
        {
            return;
        }
        m_MaxValueY = value;
        maxValueYChanged();
    }

    inline bool doesCopyY() const { return m_DoesCopyY; }
    void doesCopyY(bool value)
    {
        if (m_DoesCopyY == value)
        {
            return;
        }
        m_DoesCopyY = value;
        doesCopyYChanged();
    }

    inline bool minY() const { return m_MinY; }
    void minY(bool value)
    {
        if (m_MinY == value)
        {
            return;
        }
        m_MinY = value;
        minYChanged();
    }

    inline bool maxY() const { return m_MaxY; }
    void maxY(bool value)
    {
        if (m_MaxY == value)
        {
            return;
        }
        m_MaxY = value;
        maxYChanged();
    }

    void copy(const TransformComponentConstraintYBase& object)
    {
        m_CopyFactorY = object.m_CopyFactorY;
        m_MinValueY = object.m_MinValueY;
        m_MaxValueY = object.m_MaxValueY;
        m_DoesCopyY = object.m_DoesCopyY;
        m_MinY = object.m_MinY;
        m_MaxY = object.m_MaxY;
        TransformComponentConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case copyFactorYPropertyKey:
                m_CopyFactorY = CoreDoubleType::deserialize(reader);
                return true;
            case minValueYPropertyKey:
                m_MinValueY = CoreDoubleType::deserialize(reader);
                return true;
            case maxValueYPropertyKey:
                m_MaxValueY = CoreDoubleType::deserialize(reader);
                return true;
            case doesCopyYPropertyKey:
                m_DoesCopyY = CoreBoolType::deserialize(reader);
                return true;
            case minYPropertyKey:
                m_MinY = CoreBoolType::deserialize(reader);
                return true;
            case maxYPropertyKey:
                m_MaxY = CoreBoolType::deserialize(reader);
                return true;
        }
        return TransformComponentConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void copyFactorYChanged() {}
    virtual void minValueYChanged() {}
    virtual void maxValueYChanged() {}
    virtual void doesCopyYChanged() {}
    virtual void minYChanged() {}
    virtual void maxYChanged() {}
};
} // namespace rive

#endif