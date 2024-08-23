#ifndef _RIVE_DRAWABLE_ASSET_BASE_HPP_
#define _RIVE_DRAWABLE_ASSET_BASE_HPP_
#include "rive/assets/file_asset.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class DrawableAssetBase : public FileAsset
{
protected:
    typedef FileAsset Super;

public:
    static const uint16_t typeKey = 104;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DrawableAssetBase::typeKey:
            case FileAssetBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t heightPropertyKey = 207;
    static const uint16_t widthPropertyKey = 208;

private:
    float m_Height = 0.0f;
    float m_Width = 0.0f;

public:
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

    void copy(const DrawableAssetBase& object)
    {
        m_Height = object.m_Height;
        m_Width = object.m_Width;
        FileAsset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case heightPropertyKey:
                m_Height = CoreDoubleType::deserialize(reader);
                return true;
            case widthPropertyKey:
                m_Width = CoreDoubleType::deserialize(reader);
                return true;
        }
        return FileAsset::deserialize(propertyKey, reader);
    }

protected:
    virtual void heightChanged() {}
    virtual void widthChanged() {}
};
} // namespace rive

#endif