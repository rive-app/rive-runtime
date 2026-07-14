#ifndef _RIVE_LIBRARY_ASSET_BASE_HPP_
#define _RIVE_LIBRARY_ASSET_BASE_HPP_
#include "rive/assets/file_asset.hpp"
#include "rive/core/field_types/core_uint64_type.hpp"
namespace rive
{
class LibraryAssetBase : public FileAsset
{
protected:
    typedef FileAsset Super;

public:
    static const uint16_t typeKey = 558;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LibraryAssetBase::typeKey:
            case FileAssetBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t libraryIdPropertyKey = 798;
    static const uint16_t libraryVersionIdPropertyKey = 799;

protected:
    uint64_t m_LibraryId = 0;
    uint64_t m_LibraryVersionId = 0;

public:
    inline uint64_t libraryId() const { return m_LibraryId; }
    void libraryId(uint64_t value)
    {
        if (m_LibraryId == value)
        {
            return;
        }
        m_LibraryId = value;
        libraryIdChanged();
        notifyPropertyChanged(libraryIdPropertyKey);
    }

    inline uint64_t libraryVersionId() const { return m_LibraryVersionId; }
    void libraryVersionId(uint64_t value)
    {
        if (m_LibraryVersionId == value)
        {
            return;
        }
        m_LibraryVersionId = value;
        libraryVersionIdChanged();
        notifyPropertyChanged(libraryVersionIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const LibraryAssetBase& object)
    {
        m_LibraryId = object.m_LibraryId;
        m_LibraryVersionId = object.m_LibraryVersionId;
        FileAsset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case libraryIdPropertyKey:
                m_LibraryId = CoreUint64Type::deserialize(reader);
                return true;
            case libraryVersionIdPropertyKey:
                m_LibraryVersionId = CoreUint64Type::deserialize(reader);
                return true;
        }
        return FileAsset::deserialize(propertyKey, reader);
    }

protected:
    virtual void libraryIdChanged() {}
    virtual void libraryVersionIdChanged() {}
};
} // namespace rive

#endif