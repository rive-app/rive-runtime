#ifndef _RIVE_TRANSFORM_CONSTRAINT_BASE_HPP_
#define _RIVE_TRANSFORM_CONSTRAINT_BASE_HPP_
#include "rive/constraints/transform_space_constraint.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class TransformConstraintBase : public TransformSpaceConstraint
{
protected:
    typedef TransformSpaceConstraint Super;

public:
    static const uint16_t typeKey = 83;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransformConstraintBase::typeKey:
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

    static const uint16_t originXPropertyKey = 372;
    static const uint16_t originYPropertyKey = 373;

private:
    float m_OriginX = 0.0f;
    float m_OriginY = 0.0f;

public:
    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        m_OriginX = value;
        originXChanged();
    }

    inline float originY() const { return m_OriginY; }
    void originY(float value)
    {
        if (m_OriginY == value)
        {
            return;
        }
        m_OriginY = value;
        originYChanged();
    }

    Core* clone() const override;
    void copy(const TransformConstraintBase& object)
    {
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        TransformSpaceConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
        }
        return TransformSpaceConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void originXChanged() {}
    virtual void originYChanged() {}
};
} // namespace rive

#endif