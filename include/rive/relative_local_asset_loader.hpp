#ifndef _RIVE_RELATIVE_LOCAL_ASSET_RESOLVER_HPP_
#define _RIVE_RELATIVE_LOCAL_ASSET_RESOLVER_HPP_

#include "rive/file_asset_loader.hpp"
#include "rive/assets/file_asset.hpp"
#include <cstdio>
#include <string>
#include "rive/span.hpp"

namespace rive
{
class FileAsset;
class Factory;

/// An implementation of FileAssetLoader which finds the assets in a local
/// path relative to the original .riv file looking for them.
class RelativeLocalAssetLoader : public FileAssetLoader
{
private:
    std::string m_Path;

public:
    RelativeLocalAssetLoader(std::string filename)
    {
        std::size_t finalSlash = filename.rfind('/');

        if (finalSlash != std::string::npos)
        {
            m_Path = filename.substr(0, finalSlash + 1);
        }
    }

    bool loadContents(FileAsset& asset,
                      Span<const uint8_t> inBandBytes,
                      rive::Factory* factory) override
    {
        std::string filename = m_Path + asset.uniqueFilename();
        FILE* fp = fopen(filename.c_str(), "rb");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to find file at %s\n", filename.c_str());
            return false;
        }

        fseek(fp, 0, SEEK_END);
        const size_t length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        SimpleArray<uint8_t> bytes(length);
        if (fread(bytes.data(), 1, length, fp) == length)
        {
            asset.decode(bytes, factory);
        }
        return true;
    }
};
} // namespace rive
#endif
