#ifndef _RIVE_PAINT_IMAGE_BASE_HPP_
#define _RIVE_PAINT_IMAGE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class PaintImageBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 113;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case PaintImageBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t imageAssetIdPropertyKey = 415;
    static const uint16_t imageSamplerFilterPropertyKey = 269;
    static const uint16_t imageSamplerWrapXPropertyKey = 270;
    static const uint16_t imageSamplerWrapYPropertyKey = 271;
    static const uint16_t imageScaleXPropertyKey = 416;
    static const uint16_t imageScaleYPropertyKey = 368;
    static const uint16_t imageOffsetXPropertyKey = 369;
    static const uint16_t imageOffsetYPropertyKey = 410;
    static const uint16_t imageRotationPropertyKey = 411;
    static const uint16_t imageSizeModePropertyKey = 412;

protected:
    Id m_ImageAssetId = kEmptyId;
    uint8_t m_ImageSamplerFilter = 0;
    uint8_t m_ImageSamplerWrapX = 0;
    uint8_t m_ImageSamplerWrapY = 0;
    float m_ImageScaleX = 1.0f;
    float m_ImageScaleY = 1.0f;
    float m_ImageOffsetX = 0.0f;
    float m_ImageOffsetY = 0.0f;
    float m_ImageRotation = 0.0f;
    uint8_t m_ImageSizeMode = 0;

public:
    inline Id imageAssetId() const { return m_ImageAssetId; }
    void imageAssetId(Id value)
    {
        if (m_ImageAssetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageAssetIdPropertyKey, &m_ImageAssetId, &value);
        m_ImageAssetId = value;
        RIVE_EDITOR_CHANGED(imageAssetIdChanged());
        notifyPropertyChanged(imageAssetIdPropertyKey);
    }

    inline uint8_t imageSamplerFilter() const { return m_ImageSamplerFilter; }
    void imageSamplerFilter(uint8_t value)
    {
        if (m_ImageSamplerFilter == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageSamplerFilterPropertyKey,
                             &m_ImageSamplerFilter,
                             &value);
        m_ImageSamplerFilter = value;
        RIVE_EDITOR_CHANGED(imageSamplerFilterChanged());
        notifyPropertyChanged(imageSamplerFilterPropertyKey);
    }

    inline uint8_t imageSamplerWrapX() const { return m_ImageSamplerWrapX; }
    void imageSamplerWrapX(uint8_t value)
    {
        if (m_ImageSamplerWrapX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageSamplerWrapXPropertyKey,
                             &m_ImageSamplerWrapX,
                             &value);
        m_ImageSamplerWrapX = value;
        RIVE_EDITOR_CHANGED(imageSamplerWrapXChanged());
        notifyPropertyChanged(imageSamplerWrapXPropertyKey);
    }

    inline uint8_t imageSamplerWrapY() const { return m_ImageSamplerWrapY; }
    void imageSamplerWrapY(uint8_t value)
    {
        if (m_ImageSamplerWrapY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageSamplerWrapYPropertyKey,
                             &m_ImageSamplerWrapY,
                             &value);
        m_ImageSamplerWrapY = value;
        RIVE_EDITOR_CHANGED(imageSamplerWrapYChanged());
        notifyPropertyChanged(imageSamplerWrapYPropertyKey);
    }

    inline float imageScaleX() const { return m_ImageScaleX; }
    void imageScaleX(float value)
    {
        if (m_ImageScaleX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageScaleXPropertyKey, &m_ImageScaleX, &value);
        m_ImageScaleX = value;
        RIVE_EDITOR_CHANGED(imageScaleXChanged());
        notifyPropertyChanged(imageScaleXPropertyKey);
    }

    inline float imageScaleY() const { return m_ImageScaleY; }
    void imageScaleY(float value)
    {
        if (m_ImageScaleY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageScaleYPropertyKey, &m_ImageScaleY, &value);
        m_ImageScaleY = value;
        RIVE_EDITOR_CHANGED(imageScaleYChanged());
        notifyPropertyChanged(imageScaleYPropertyKey);
    }

    inline float imageOffsetX() const { return m_ImageOffsetX; }
    void imageOffsetX(float value)
    {
        if (m_ImageOffsetX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageOffsetXPropertyKey, &m_ImageOffsetX, &value);
        m_ImageOffsetX = value;
        RIVE_EDITOR_CHANGED(imageOffsetXChanged());
        notifyPropertyChanged(imageOffsetXPropertyKey);
    }

    inline float imageOffsetY() const { return m_ImageOffsetY; }
    void imageOffsetY(float value)
    {
        if (m_ImageOffsetY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageOffsetYPropertyKey, &m_ImageOffsetY, &value);
        m_ImageOffsetY = value;
        RIVE_EDITOR_CHANGED(imageOffsetYChanged());
        notifyPropertyChanged(imageOffsetYPropertyKey);
    }

    inline float imageRotation() const { return m_ImageRotation; }
    void imageRotation(float value)
    {
        if (m_ImageRotation == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageRotationPropertyKey,
                             &m_ImageRotation,
                             &value);
        m_ImageRotation = value;
        RIVE_EDITOR_CHANGED(imageRotationChanged());
        notifyPropertyChanged(imageRotationPropertyKey);
    }

    inline uint8_t imageSizeMode() const { return m_ImageSizeMode; }
    void imageSizeMode(uint8_t value)
    {
        if (m_ImageSizeMode == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(imageSizeModePropertyKey,
                             &m_ImageSizeMode,
                             &value);
        m_ImageSizeMode = value;
        RIVE_EDITOR_CHANGED(imageSizeModeChanged());
        notifyPropertyChanged(imageSizeModePropertyKey);
    }

    Core* clone() const override;
    void copy(const PaintImageBase& object)
    {
        m_ImageAssetId = object.m_ImageAssetId;
        m_ImageSamplerFilter = object.m_ImageSamplerFilter;
        m_ImageSamplerWrapX = object.m_ImageSamplerWrapX;
        m_ImageSamplerWrapY = object.m_ImageSamplerWrapY;
        m_ImageScaleX = object.m_ImageScaleX;
        m_ImageScaleY = object.m_ImageScaleY;
        m_ImageOffsetX = object.m_ImageOffsetX;
        m_ImageOffsetY = object.m_ImageOffsetY;
        m_ImageRotation = object.m_ImageRotation;
        m_ImageSizeMode = object.m_ImageSizeMode;
        RIVE_EDITOR_COPY(object);
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case imageAssetIdPropertyKey:
                m_ImageAssetId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case imageSamplerFilterPropertyKey:
                m_ImageSamplerFilter = CoreUintType::deserialize(reader);
                return true;
            case imageSamplerWrapXPropertyKey:
                m_ImageSamplerWrapX = CoreUintType::deserialize(reader);
                return true;
            case imageSamplerWrapYPropertyKey:
                m_ImageSamplerWrapY = CoreUintType::deserialize(reader);
                return true;
            case imageScaleXPropertyKey:
                m_ImageScaleX = CoreDoubleType::deserialize(reader);
                return true;
            case imageScaleYPropertyKey:
                m_ImageScaleY = CoreDoubleType::deserialize(reader);
                return true;
            case imageOffsetXPropertyKey:
                m_ImageOffsetX = CoreDoubleType::deserialize(reader);
                return true;
            case imageOffsetYPropertyKey:
                m_ImageOffsetY = CoreDoubleType::deserialize(reader);
                return true;
            case imageRotationPropertyKey:
                m_ImageRotation = CoreDoubleType::deserialize(reader);
                return true;
            case imageSizeModePropertyKey:
                m_ImageSizeMode = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void imageAssetIdChanged() {}
    virtual void imageSamplerFilterChanged() {}
    virtual void imageSamplerWrapXChanged() {}
    virtual void imageSamplerWrapYChanged() {}
    virtual void imageScaleXChanged() {}
    virtual void imageScaleYChanged() {}
    virtual void imageOffsetXChanged() {}
    virtual void imageOffsetYChanged() {}
    virtual void imageRotationChanged() {}
    virtual void imageSizeModeChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/paint/paint_image_ext.inl"
#endif
};
} // namespace rive

#endif