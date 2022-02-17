#ifndef _RIVE_FILE_ASSET_HPP_
#define _RIVE_FILE_ASSET_HPP_
#include "rive/generated/assets/file_asset_base.hpp"
#include <string>

namespace rive {
    class FileAsset : public FileAssetBase {
    public:
        virtual bool decode(const uint8_t* bytes, std::size_t size) = 0;
        virtual std::string fileExtension() = 0;
        StatusCode import(ImportStack& importStack) override;

        std::string uniqueFilename();
    };
} // namespace rive

#endif