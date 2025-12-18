#ifndef _RIVE_MANIFEST_ASSET_HPP_
#define _RIVE_MANIFEST_ASSET_HPP_
#include "rive/generated/assets/manifest_asset_base.hpp"
#include "rive/name_resolver.hpp"
#include <stdio.h>
#include <unordered_map>
#include <string>
namespace rive
{
class ManifestAsset : public ManifestAssetBase, public NameResolver
{
private:
    std::unordered_map<int, std::string> m_names;
    static const std::string empty;

protected:
    bool addsToBackboard() override { return false; }

public:
    bool decode(SimpleArray<uint8_t>&, Factory*) override;
    std::string fileExtension() const override;
    const std::string& resolveName(int id) override;
};
} // namespace rive

#endif