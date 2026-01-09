#ifndef _RIVE_MANIFEST_ASSET_HPP_
#define _RIVE_MANIFEST_ASSET_HPP_
#include "rive/generated/assets/manifest_asset_base.hpp"
#include "rive/data_resolver.hpp"
#include <stdio.h>
#include <unordered_map>
#include <string>
namespace rive
{
class ManifestAsset : public ManifestAssetBase, public DataResolver
{
private:
    std::unordered_map<int, std::string> m_names;
    std::unordered_map<int, std::vector<uint32_t>> m_paths;
    static const std::string empty;
    static const std::vector<uint32_t> emptyIntVector;
    bool decodeNames(BinaryReader& reader);
    bool decodePaths(BinaryReader& reader);

protected:
    bool addsToBackboard() override { return false; }

public:
    bool decode(SimpleArray<uint8_t>&, Factory*) override;
    std::string fileExtension() const override;
    const std::string& resolveName(int id) override;
    const std::vector<uint32_t>& resolvePath(int id) override;
};
} // namespace rive

#endif