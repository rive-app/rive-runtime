#ifndef _RIVE_BITMAP_CACHE_BASE_HPP_
#define _RIVE_BITMAP_CACHE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class BitmapCacheBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 136;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BitmapCacheBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t resolutionPropertyKey = 417;
    static const uint16_t cacheFlagsPropertyKey = 418;
    static const uint16_t cacheEnabledPropertyKey = 419;
    static const uint32_t cacheEnabledBitmask = 1u << 0;
    static const uint16_t ditherPropertyKey = 420;
    static const uint32_t ditherBitmask = 1u << 1;

protected:
    float m_Resolution = 1.0f;
    uint32_t m_CacheFlags = 1;

public:
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

    inline uint32_t cacheFlags() const { return m_CacheFlags; }
    void cacheFlags(uint32_t value)
    {
        if (m_CacheFlags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cacheFlagsPropertyKey, &m_CacheFlags, &value);
        m_CacheFlags = value;
        RIVE_EDITOR_CHANGED(cacheFlagsChanged());
        notifyPropertyChanged(cacheFlagsPropertyKey);
    }

    inline bool cacheEnabled() const
    {
        return (m_CacheFlags & cacheEnabledBitmask) != 0;
    }
    void cacheEnabled(bool value)
    {
        const bool prev = (m_CacheFlags & cacheEnabledBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cacheEnabledPropertyKey, &prev, &value);
        m_CacheFlags = value ? (m_CacheFlags | cacheEnabledBitmask)
                             : (m_CacheFlags & ~cacheEnabledBitmask);
        RIVE_EDITOR_CHANGED(cacheFlagsChanged());
        notifyPropertyChanged(cacheFlagsPropertyKey);
    }
    inline bool dither() const { return (m_CacheFlags & ditherBitmask) != 0; }
    void dither(bool value)
    {
        const bool prev = (m_CacheFlags & ditherBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(ditherPropertyKey, &prev, &value);
        m_CacheFlags = value ? (m_CacheFlags | ditherBitmask)
                             : (m_CacheFlags & ~ditherBitmask);
        RIVE_EDITOR_CHANGED(cacheFlagsChanged());
        notifyPropertyChanged(cacheFlagsPropertyKey);
    }
    Core* clone() const override;
    void copy(const BitmapCacheBase& object)
    {
        m_Resolution = object.m_Resolution;
        m_CacheFlags = object.m_CacheFlags;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case resolutionPropertyKey:
                m_Resolution = CoreDoubleType::deserialize(reader);
                return true;
            case cacheFlagsPropertyKey:
                m_CacheFlags = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void resolutionChanged() {}
    virtual void cacheFlagsChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/bitmap_cache_ext.inl"
#endif
};
} // namespace rive

#endif