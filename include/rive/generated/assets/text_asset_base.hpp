#ifndef _RIVE_TEXT_ASSET_BASE_HPP_
#define _RIVE_TEXT_ASSET_BASE_HPP_
#include <string>
#include "rive/assets/file_asset.hpp"
#include "rive/core/field_types/core_string_type.hpp"
namespace rive
{
class TextAssetBase : public FileAsset
{
protected:
    typedef FileAsset Super;

public:
    static const uint16_t typeKey = 971;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextAssetBase::typeKey:
            case FileAssetBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t folderPathPropertyKey = 926;

protected:
    std::string m_FolderPath = "";

public:
    inline const std::string& folderPath() const { return m_FolderPath; }
    void folderPath(std::string value)
    {
        if (m_FolderPath == value)
        {
            return;
        }
        m_FolderPath = value;
        folderPathChanged();
    }

    void copy(const TextAssetBase& object)
    {
        m_FolderPath = object.m_FolderPath;
        FileAsset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case folderPathPropertyKey:
                m_FolderPath = CoreStringType::deserialize(reader);
                return true;
        }
        return FileAsset::deserialize(propertyKey, reader);
    }

protected:
    virtual void folderPathChanged() {}
};
} // namespace rive

#endif