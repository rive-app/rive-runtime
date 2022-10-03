#ifndef _RIVE_FILE_ASSET_BASE_HPP_
#define _RIVE_FILE_ASSET_BASE_HPP_
#include "rive/assets/asset.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class FileAssetBase : public Asset
{
protected:
    typedef Asset Super;

public:
    static const uint16_t typeKey = 103;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FileAssetBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t assetIdPropertyKey = 204;

private:
    uint32_t m_AssetId = 0;

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

    void copy(const FileAssetBase& object)
    {
        m_AssetId = object.m_AssetId;
        Asset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case assetIdPropertyKey:
                m_AssetId = CoreUintType::deserialize(reader);
                return true;
        }
        return Asset::deserialize(propertyKey, reader);
    }

protected:
    virtual void assetIdChanged() {}
};
} // namespace rive

#endif