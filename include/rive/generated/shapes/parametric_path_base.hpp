#ifndef _RIVE_PARAMETRIC_PATH_BASE_HPP_
#define _RIVE_PARAMETRIC_PATH_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/path.hpp"
namespace rive
{
class ParametricPathBase : public Path
{
protected:
    typedef Path Super;

public:
    static const uint16_t typeKey = 15;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ParametricPathBase::typeKey:
            case PathBase::typeKey:
            case NodeBase::typeKey:
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t widthPropertyKey = 20;
    static const uint16_t heightPropertyKey = 21;
    static const uint16_t originXPropertyKey = 123;
    static const uint16_t originYPropertyKey = 124;

private:
    float m_Width = 0.0f;
    float m_Height = 0.0f;
    float m_OriginX = 0.5f;
    float m_OriginY = 0.5f;

public:
    inline float width() const { return m_Width; }
    void width(float value)
    {
        if (m_Width == value)
        {
            return;
        }
        m_Width = value;
        widthChanged();
    }

    inline float height() const { return m_Height; }
    void height(float value)
    {
        if (m_Height == value)
        {
            return;
        }
        m_Height = value;
        heightChanged();
    }

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

    void copy(const ParametricPathBase& object)
    {
        m_Width = object.m_Width;
        m_Height = object.m_Height;
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        Path::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case widthPropertyKey:
                m_Width = CoreDoubleType::deserialize(reader);
                return true;
            case heightPropertyKey:
                m_Height = CoreDoubleType::deserialize(reader);
                return true;
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Path::deserialize(propertyKey, reader);
    }

protected:
    virtual void widthChanged() {}
    virtual void heightChanged() {}
    virtual void originXChanged() {}
    virtual void originYChanged() {}
};
} // namespace rive

#endif