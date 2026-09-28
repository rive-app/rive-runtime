#ifndef _RIVE_LAYER_MASK_BASE_HPP_
#define _RIVE_LAYER_MASK_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class LayerMaskBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 154;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayerMaskBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t sourceIdPropertyKey = 459;
    static const uint16_t maskFlagsPropertyKey = 460;
    static const uint16_t maskModeValuePropertyKey = 455;
    static const uint32_t maskModeValueBitOffset = 0;
    static const uint32_t maskModeValueFieldMask = 3u;
    static const uint16_t isVisiblePropertyKey = 456;
    static const uint32_t isVisibleBitmask = 1u << 2;
    static const uint16_t sourceDrawsPropertyKey = 457;
    static const uint32_t sourceDrawsBitmask = 1u << 3;
    static const uint16_t useCustomBoundsPropertyKey = 461;
    static const uint32_t useCustomBoundsBitmask = 1u << 4;
    static const uint16_t resolutionPropertyKey = 458;
    static const uint16_t boundsXPropertyKey = 462;
    static const uint16_t boundsYPropertyKey = 463;
    static const uint16_t boundsWidthPropertyKey = 464;
    static const uint16_t boundsHeightPropertyKey = 465;

protected:
    Id m_SourceId = kEmptyId;
    uint32_t m_MaskFlags = 4;
    float m_Resolution = 1.0f;
    float m_BoundsX = 0.0f;
    float m_BoundsY = 0.0f;
    float m_BoundsWidth = 0.0f;
    float m_BoundsHeight = 0.0f;

public:
    inline Id sourceId() const { return m_SourceId; }
    void sourceId(Id value)
    {
        if (m_SourceId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(sourceIdPropertyKey, &m_SourceId, &value);
        m_SourceId = value;
        RIVE_EDITOR_CHANGED(sourceIdChanged());
        notifyPropertyChanged(sourceIdPropertyKey);
    }

    inline uint32_t maskFlags() const { return m_MaskFlags; }
    void maskFlags(uint32_t value)
    {
        if (m_MaskFlags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maskFlagsPropertyKey, &m_MaskFlags, &value);
        m_MaskFlags = value;
        RIVE_EDITOR_CHANGED(maskFlagsChanged());
        notifyPropertyChanged(maskFlagsPropertyKey);
    }

    inline uint8_t maskModeValue() const
    {
        return (m_MaskFlags & maskModeValueFieldMask) >> maskModeValueBitOffset;
    }
    void maskModeValue(uint8_t value)
    {
        const uint8_t prev =
            (m_MaskFlags & maskModeValueFieldMask) >> maskModeValueBitOffset;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maskModeValuePropertyKey, &prev, &value);
        m_MaskFlags =
            (m_MaskFlags & ~maskModeValueFieldMask) |
            ((value << maskModeValueBitOffset) & maskModeValueFieldMask);
        RIVE_EDITOR_CHANGED(maskFlagsChanged());
        notifyPropertyChanged(maskFlagsPropertyKey);
    }
    inline bool isVisible() const
    {
        return (m_MaskFlags & isVisibleBitmask) != 0;
    }
    void isVisible(bool value)
    {
        const bool prev = (m_MaskFlags & isVisibleBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isVisiblePropertyKey, &prev, &value);
        m_MaskFlags = value ? (m_MaskFlags | isVisibleBitmask)
                            : (m_MaskFlags & ~isVisibleBitmask);
        RIVE_EDITOR_CHANGED(maskFlagsChanged());
        notifyPropertyChanged(maskFlagsPropertyKey);
    }
    inline bool sourceDraws() const
    {
        return (m_MaskFlags & sourceDrawsBitmask) != 0;
    }
    void sourceDraws(bool value)
    {
        const bool prev = (m_MaskFlags & sourceDrawsBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(sourceDrawsPropertyKey, &prev, &value);
        m_MaskFlags = value ? (m_MaskFlags | sourceDrawsBitmask)
                            : (m_MaskFlags & ~sourceDrawsBitmask);
        RIVE_EDITOR_CHANGED(maskFlagsChanged());
        notifyPropertyChanged(maskFlagsPropertyKey);
    }
    inline bool useCustomBounds() const
    {
        return (m_MaskFlags & useCustomBoundsBitmask) != 0;
    }
    void useCustomBounds(bool value)
    {
        const bool prev = (m_MaskFlags & useCustomBoundsBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(useCustomBoundsPropertyKey, &prev, &value);
        m_MaskFlags = value ? (m_MaskFlags | useCustomBoundsBitmask)
                            : (m_MaskFlags & ~useCustomBoundsBitmask);
        RIVE_EDITOR_CHANGED(maskFlagsChanged());
        notifyPropertyChanged(maskFlagsPropertyKey);
    }
    inline float resolution() const { return m_Resolution; }
    void resolution(float value)
    {
        if (m_Resolution == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(resolutionPropertyKey, &m_Resolution, &value);
        m_Resolution = value;
        RIVE_EDITOR_CHANGED(resolutionChanged());
        notifyPropertyChanged(resolutionPropertyKey);
    }

    inline float boundsX() const { return m_BoundsX; }
    void boundsX(float value)
    {
        if (m_BoundsX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(boundsXPropertyKey, &m_BoundsX, &value);
        m_BoundsX = value;
        RIVE_EDITOR_CHANGED(boundsXChanged());
        notifyPropertyChanged(boundsXPropertyKey);
    }

    inline float boundsY() const { return m_BoundsY; }
    void boundsY(float value)
    {
        if (m_BoundsY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(boundsYPropertyKey, &m_BoundsY, &value);
        m_BoundsY = value;
        RIVE_EDITOR_CHANGED(boundsYChanged());
        notifyPropertyChanged(boundsYPropertyKey);
    }

    inline float boundsWidth() const { return m_BoundsWidth; }
    void boundsWidth(float value)
    {
        if (m_BoundsWidth == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(boundsWidthPropertyKey, &m_BoundsWidth, &value);
        m_BoundsWidth = value;
        RIVE_EDITOR_CHANGED(boundsWidthChanged());
        notifyPropertyChanged(boundsWidthPropertyKey);
    }

    inline float boundsHeight() const { return m_BoundsHeight; }
    void boundsHeight(float value)
    {
        if (m_BoundsHeight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(boundsHeightPropertyKey, &m_BoundsHeight, &value);
        m_BoundsHeight = value;
        RIVE_EDITOR_CHANGED(boundsHeightChanged());
        notifyPropertyChanged(boundsHeightPropertyKey);
    }

    Core* clone() const override;
    void copy(const LayerMaskBase& object)
    {
        m_SourceId = object.m_SourceId;
        m_MaskFlags = object.m_MaskFlags;
        m_Resolution = object.m_Resolution;
        m_BoundsX = object.m_BoundsX;
        m_BoundsY = object.m_BoundsY;
        m_BoundsWidth = object.m_BoundsWidth;
        m_BoundsHeight = object.m_BoundsHeight;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case sourceIdPropertyKey:
                m_SourceId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case maskFlagsPropertyKey:
                m_MaskFlags = CoreUintType::deserialize(reader);
                return true;
            case resolutionPropertyKey:
                m_Resolution = CoreDoubleType::deserialize(reader);
                return true;
            case boundsXPropertyKey:
                m_BoundsX = CoreDoubleType::deserialize(reader);
                return true;
            case boundsYPropertyKey:
                m_BoundsY = CoreDoubleType::deserialize(reader);
                return true;
            case boundsWidthPropertyKey:
                m_BoundsWidth = CoreDoubleType::deserialize(reader);
                return true;
            case boundsHeightPropertyKey:
                m_BoundsHeight = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void sourceIdChanged() {}
    virtual void maskFlagsChanged() {}
    virtual void resolutionChanged() {}
    virtual void boundsXChanged() {}
    virtual void boundsYChanged() {}
    virtual void boundsWidthChanged() {}
    virtual void boundsHeightChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layer_mask_ext.inl"
#endif
};
} // namespace rive

#endif