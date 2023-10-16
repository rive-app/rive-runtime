#ifndef _RIVE_FILE_ASSET_HPP_
#define _RIVE_FILE_ASSET_HPP_
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

public:
    Span<const uint8_t> cdnUuid() const;
    std::string cdnUuidStr() const;

    void decodeCdnUuid(Span<const uint8_t> value) override;
    void copyCdnUuid(const FileAssetBase& object) override;
    virtual bool decode(Span<const uint8_t>, Factory*) = 0;
    virtual std::string fileExtension() const = 0;
    StatusCode import(ImportStack& importStack) override;

    std::string uniqueFilename() const;
};
} // namespace rive

#endif