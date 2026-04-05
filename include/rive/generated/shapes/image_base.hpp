#ifndef _RIVE_IMAGE_BASE_HPP_
#define _RIVE_IMAGE_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/drawable.hpp"
namespace rive
{
class ImageBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 100;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ImageBase::typeKey:
            case DrawableBase::typeKey:
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

    static const uint16_t assetIdPropertyKey = 206;
    static const uint16_t originXPropertyKey = 380;
    static const uint16_t originYPropertyKey = 381;
    static const uint16_t fitPropertyKey = 974;
    static const uint16_t alignmentXPropertyKey = 975;
    static const uint16_t alignmentYPropertyKey = 976;

protected:
    uint32_t m_AssetId = -1;
    float m_OriginX = 0.5f;
    float m_OriginY = 0.5f;
    uint32_t m_Fit = 0;
    float m_AlignmentX = 0.0f;
    float m_AlignmentY = 0.0f;

public:
    inline uint32_t assetId() const { return m_AssetId; }
    void assetId(uint32_t value)
    {
        if (m_AssetId == value)
        {
            return;
        }
        m_AssetId = value;
        assetIdChanged();
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

    inline uint32_t fit() const { return m_Fit; }
    void fit(uint32_t value)
    {
        if (m_Fit == value)
        {
            return;
        }
        m_Fit = value;
        fitChanged();
    }

    inline float alignmentX() const { return m_AlignmentX; }
    void alignmentX(float value)
    {
        if (m_AlignmentX == value)
        {
            return;
        }
        m_AlignmentX = value;
        alignmentXChanged();
    }

    inline float alignmentY() const { return m_AlignmentY; }
    void alignmentY(float value)
    {
        if (m_AlignmentY == value)
        {
            return;
        }
        m_AlignmentY = value;
        alignmentYChanged();
    }

    Core* clone() const override;
    void copy(const ImageBase& object)
    {
        m_AssetId = object.m_AssetId;
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        m_Fit = object.m_Fit;
        m_AlignmentX = object.m_AlignmentX;
        m_AlignmentY = object.m_AlignmentY;
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case assetIdPropertyKey:
                m_AssetId = CoreUintType::deserialize(reader);
                return true;
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
            case fitPropertyKey:
                m_Fit = CoreUintType::deserialize(reader);
                return true;
            case alignmentXPropertyKey:
                m_AlignmentX = CoreDoubleType::deserialize(reader);
                return true;
            case alignmentYPropertyKey:
                m_AlignmentY = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void assetIdChanged() {}
    virtual void originXChanged() {}
    virtual void originYChanged() {}
    virtual void fitChanged() {}
    virtual void alignmentXChanged() {}
    virtual void alignmentYChanged() {}
};
} // namespace rive

#endif