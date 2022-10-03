#ifndef _RIVE_STROKE_BASE_HPP_
#define _RIVE_STROKE_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
namespace rive
{
class StrokeBase : public ShapePaint
{
protected:
    typedef ShapePaint Super;

public:
    static const uint16_t typeKey = 24;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StrokeBase::typeKey:
            case ShapePaintBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t thicknessPropertyKey = 47;
    static const uint16_t capPropertyKey = 48;
    static const uint16_t joinPropertyKey = 49;
    static const uint16_t transformAffectsStrokePropertyKey = 50;

private:
    float m_Thickness = 1.0f;
    uint32_t m_Cap = 0;
    uint32_t m_Join = 0;
    bool m_TransformAffectsStroke = true;

public:
    inline float thickness() const { return m_Thickness; }
    void thickness(float value)
    {
        if (m_Thickness == value)
        {
            return;
        }
        m_Thickness = value;
        thicknessChanged();
    }

    inline uint32_t cap() const { return m_Cap; }
    void cap(uint32_t value)
    {
        if (m_Cap == value)
        {
            return;
        }
        m_Cap = value;
        capChanged();
    }

    inline uint32_t join() const { return m_Join; }
    void join(uint32_t value)
    {
        if (m_Join == value)
        {
            return;
        }
        m_Join = value;
        joinChanged();
    }

    inline bool transformAffectsStroke() const { return m_TransformAffectsStroke; }
    void transformAffectsStroke(bool value)
    {
        if (m_TransformAffectsStroke == value)
        {
            return;
        }
        m_TransformAffectsStroke = value;
        transformAffectsStrokeChanged();
    }

    Core* clone() const override;
    void copy(const StrokeBase& object)
    {
        m_Thickness = object.m_Thickness;
        m_Cap = object.m_Cap;
        m_Join = object.m_Join;
        m_TransformAffectsStroke = object.m_TransformAffectsStroke;
        ShapePaint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case thicknessPropertyKey:
                m_Thickness = CoreDoubleType::deserialize(reader);
                return true;
            case capPropertyKey:
                m_Cap = CoreUintType::deserialize(reader);
                return true;
            case joinPropertyKey:
                m_Join = CoreUintType::deserialize(reader);
                return true;
            case transformAffectsStrokePropertyKey:
                m_TransformAffectsStroke = CoreBoolType::deserialize(reader);
                return true;
        }
        return ShapePaint::deserialize(propertyKey, reader);
    }

protected:
    virtual void thicknessChanged() {}
    virtual void capChanged() {}
    virtual void joinChanged() {}
    virtual void transformAffectsStrokeChanged() {}
};
} // namespace rive

#endif