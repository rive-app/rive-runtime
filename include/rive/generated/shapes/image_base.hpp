#ifndef _RIVE_IMAGE_BASE_HPP_
#define _RIVE_IMAGE_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
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
    static const uint16_t samplerFilterPropertyKey = 1076;
    static const uint16_t samplerWrapXPropertyKey = 1077;
    static const uint16_t samplerWrapYPropertyKey = 1078;

protected:
    Id m_AssetId = kEmptyId;
    float m_OriginX = 0.5f;
    float m_OriginY = 0.5f;
    uint32_t m_Fit = 0;
    float m_AlignmentX = 0.0f;
    float m_AlignmentY = 0.0f;
    uint8_t m_SamplerFilter = 0;
    uint8_t m_SamplerWrapX = 0;
    uint8_t m_SamplerWrapY = 0;

public:
    inline Id assetId() const { return m_AssetId; }
    void assetId(Id value)
    {
        if (m_AssetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(assetIdPropertyKey, &m_AssetId, &value);
        m_AssetId = value;
        RIVE_EDITOR_CHANGED(assetIdChanged());
        notifyPropertyChanged(assetIdPropertyKey);
    }

    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originXPropertyKey, &m_OriginX, &value);
        m_OriginX = value;
        RIVE_EDITOR_CHANGED(originXChanged());
        notifyPropertyChanged(originXPropertyKey);
    }

    inline float originY() const { return m_OriginY; }
    void originY(float value)
    {
        if (m_OriginY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originYPropertyKey, &m_OriginY, &value);
        m_OriginY = value;
        RIVE_EDITOR_CHANGED(originYChanged());
        notifyPropertyChanged(originYPropertyKey);
    }

    inline uint32_t fit() const { return m_Fit; }
    void fit(uint32_t value)
    {
        if (m_Fit == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(fitPropertyKey, &m_Fit, &value);
        m_Fit = value;
        RIVE_EDITOR_CHANGED(fitChanged());
        notifyPropertyChanged(fitPropertyKey);
    }

    inline float alignmentX() const { return m_AlignmentX; }
    void alignmentX(float value)
    {
        if (m_AlignmentX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(alignmentXPropertyKey, &m_AlignmentX, &value);
        m_AlignmentX = value;
        RIVE_EDITOR_CHANGED(alignmentXChanged());
        notifyPropertyChanged(alignmentXPropertyKey);
    }

    inline float alignmentY() const { return m_AlignmentY; }
    void alignmentY(float value)
    {
        if (m_AlignmentY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(alignmentYPropertyKey, &m_AlignmentY, &value);
        m_AlignmentY = value;
        RIVE_EDITOR_CHANGED(alignmentYChanged());
        notifyPropertyChanged(alignmentYPropertyKey);
    }

    inline uint8_t samplerFilter() const { return m_SamplerFilter; }
    void samplerFilter(uint8_t value)
    {
        if (m_SamplerFilter == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(samplerFilterPropertyKey,
                             &m_SamplerFilter,
                             &value);
        m_SamplerFilter = value;
        RIVE_EDITOR_CHANGED(samplerFilterChanged());
        notifyPropertyChanged(samplerFilterPropertyKey);
    }

    inline uint8_t samplerWrapX() const { return m_SamplerWrapX; }
    void samplerWrapX(uint8_t value)
    {
        if (m_SamplerWrapX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(samplerWrapXPropertyKey, &m_SamplerWrapX, &value);
        m_SamplerWrapX = value;
        RIVE_EDITOR_CHANGED(samplerWrapXChanged());
        notifyPropertyChanged(samplerWrapXPropertyKey);
    }

    inline uint8_t samplerWrapY() const { return m_SamplerWrapY; }
    void samplerWrapY(uint8_t value)
    {
        if (m_SamplerWrapY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(samplerWrapYPropertyKey, &m_SamplerWrapY, &value);
        m_SamplerWrapY = value;
        RIVE_EDITOR_CHANGED(samplerWrapYChanged());
        notifyPropertyChanged(samplerWrapYPropertyKey);
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
        m_SamplerFilter = object.m_SamplerFilter;
        m_SamplerWrapX = object.m_SamplerWrapX;
        m_SamplerWrapY = object.m_SamplerWrapY;
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case assetIdPropertyKey:
                m_AssetId = CoreIdType::runtimeDeserialize(reader);
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
            case samplerFilterPropertyKey:
                m_SamplerFilter = CoreUintType::deserialize(reader);
                return true;
            case samplerWrapXPropertyKey:
                m_SamplerWrapX = CoreUintType::deserialize(reader);
                return true;
            case samplerWrapYPropertyKey:
                m_SamplerWrapY = CoreUintType::deserialize(reader);
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
    virtual void samplerFilterChanged() {}
    virtual void samplerWrapXChanged() {}
    virtual void samplerWrapYChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/image_ext.inl"
#endif
};
} // namespace rive

#endif