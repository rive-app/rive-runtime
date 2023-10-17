#ifndef _RIVE_FILE_ASSET_HPP_
#define _RIVE_FILE_ASSET_HPP_
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/generated/assets/file_asset_base.hpp"
#include "rive/span.hpp"
#include <string>

namespace rive
{
class Factory;
class FileAsset : public FileAssetBase
{
private:
    std::vector<uint8_t> m_cdnUuid;
    std::vector<FileAssetReferencer*> m_fileAssetReferencers;

public:
    Span<const uint8_t> cdnUuid() const;
    std::string cdnUuidStr() const;

    void decodeCdnUuid(Span<const uint8_t> value) override;
    void copyCdnUuid(const FileAssetBase& object) override;
    virtual bool decode(Span<const uint8_t>, Factory*) = 0;
    virtual std::string fileExtension() const = 0;
    StatusCode import(ImportStack& importStack) override;
    const std::vector<FileAssetReferencer*> fileAssetReferencers()
    {
        return m_fileAssetReferencers;
    }

    void addFileAssetReferencer(FileAssetReferencer* referencer)
    {
        m_fileAssetReferencers.push_back(referencer);
    }

    void removeFileAssetReferencer(FileAssetReferencer* referencer)
    {
        m_fileAssetReferencers.erase(
            std::remove_if(m_fileAssetReferencers.begin(),
                           m_fileAssetReferencers.end(),
                           [&referencer](FileAssetReferencer* otherReferencer) {
                               return otherReferencer == referencer;
                           }),
            m_fileAssetReferencers.end());
    }

    std::string uniqueFilename() const;
};
} // namespace rive

#endif