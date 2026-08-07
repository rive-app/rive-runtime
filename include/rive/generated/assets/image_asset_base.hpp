#ifndef _RIVE_IMAGE_ASSET_BASE_HPP_
#define _RIVE_IMAGE_ASSET_BASE_HPP_
#include "rive/assets/drawable_asset.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ImageAssetBase : public DrawableAsset
{
protected:
    typedef DrawableAsset Super;

public:
    static const uint16_t typeKey = 105;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ImageAssetBase::typeKey:
            case DrawableAssetBase::typeKey:
            case FileAssetBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

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
        m_SamplerFilter = value;
        samplerFilterChanged();
        notifyPropertyChanged(samplerFilterPropertyKey);
    }

    inline uint8_t samplerWrapX() const { return m_SamplerWrapX; }
    void samplerWrapX(uint8_t value)
    {
        if (m_SamplerWrapX == value)
        {
            return;
        }
        m_SamplerWrapX = value;
        samplerWrapXChanged();
        notifyPropertyChanged(samplerWrapXPropertyKey);
    }

    inline uint8_t samplerWrapY() const { return m_SamplerWrapY; }
    void samplerWrapY(uint8_t value)
    {
        if (m_SamplerWrapY == value)
        {
            return;
        }
        m_SamplerWrapY = value;
        samplerWrapYChanged();
        notifyPropertyChanged(samplerWrapYPropertyKey);
    }

    Core* clone() const override;
    void copy(const ImageAssetBase& object)
    {
        m_SamplerFilter = object.m_SamplerFilter;
        m_SamplerWrapX = object.m_SamplerWrapX;
        m_SamplerWrapY = object.m_SamplerWrapY;
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
        return DrawableAsset::deserialize(propertyKey, reader);
    }

protected:
    virtual void samplerFilterChanged() {}
    virtual void samplerWrapXChanged() {}
    virtual void samplerWrapYChanged() {}
};
} // namespace rive

#endif