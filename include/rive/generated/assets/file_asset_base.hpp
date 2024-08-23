#ifndef _RIVE_FILE_ASSET_BASE_HPP_
#define _RIVE_FILE_ASSET_BASE_HPP_
#include <string>
#include "rive/assets/asset.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/span.hpp"
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
    static const uint16_t cdnUuidPropertyKey = 359;
    static const uint16_t cdnBaseUrlPropertyKey = 362;

private:
    uint32_t m_AssetId = 0;
    std::string m_CdnBaseUrl = "https://public.rive.app/cdn/uuid";

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

    virtual void decodeCdnUuid(Span<const uint8_t> value) = 0;
    virtual void copyCdnUuid(const FileAssetBase& object) = 0;

    inline const std::string& cdnBaseUrl() const { return m_CdnBaseUrl; }
    void cdnBaseUrl(std::string value)
    {
        if (m_CdnBaseUrl == value)
        {
            return;
        }
        m_CdnBaseUrl = value;
        cdnBaseUrlChanged();
    }

    void copy(const FileAssetBase& object)
    {
        m_AssetId = object.m_AssetId;
        copyCdnUuid(object);
        m_CdnBaseUrl = object.m_CdnBaseUrl;
        Asset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case assetIdPropertyKey:
                m_AssetId = CoreUintType::deserialize(reader);
                return true;
            case cdnUuidPropertyKey:
                decodeCdnUuid(CoreBytesType::deserialize(reader));
                return true;
            case cdnBaseUrlPropertyKey:
                m_CdnBaseUrl = CoreStringType::deserialize(reader);
                return true;
        }
        return Asset::deserialize(propertyKey, reader);
    }

protected:
    virtual void assetIdChanged() {}
    virtual void cdnUuidChanged() {}
    virtual void cdnBaseUrlChanged() {}
};
} // namespace rive

#endif