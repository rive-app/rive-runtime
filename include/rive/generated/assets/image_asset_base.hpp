#ifndef _RIVE_IMAGE_ASSET_BASE_HPP_
#define _RIVE_IMAGE_ASSET_BASE_HPP_
#include "rive/assets/drawable_asset.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ImageAssetBase : public DrawableAsset
{
protected:
    typedef DrawableAsset Super;

public:
    static const uint16_t typeKey = 105;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t samplerFilterPropertyKey = 1073;
    static const uint16_t samplerWrapXPropertyKey = 1074;
    static const uint16_t samplerWrapYPropertyKey = 1075;

protected:
    uint8_t m_SamplerFilter = 0;
    uint8_t m_SamplerWrapX = 0;
    uint8_t m_SamplerWrapY = 0;

public:
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
        notifyPropertyChanged(samplerWrapYPropertyKey);
    }

    Core* clone() const override;
    void copy(const ImageAssetBase& object)
    {
        m_SamplerFilter = object.m_SamplerFilter;
        m_SamplerWrapX = object.m_SamplerWrapX;
        m_SamplerWrapY = object.m_SamplerWrapY;
        RIVE_EDITOR_COPY(object);
        DrawableAsset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
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
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return DrawableAsset::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/assets/image_asset_ext.inl"
#endif
};
} // namespace rive

#endif