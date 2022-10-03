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
public:
    virtual bool decode(Span<const uint8_t>, Factory*) = 0;
    virtual std::string fileExtension() = 0;
    StatusCode import(ImportStack& importStack) override;

    std::string uniqueFilename();
};
} // namespace rive

#endif