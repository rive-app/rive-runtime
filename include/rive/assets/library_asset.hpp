#ifndef _RIVE_LIBRARY_ASSET_HPP_
#define _RIVE_LIBRARY_ASSET_HPP_
#include "rive/generated/assets/library_asset_base.hpp"
namespace rive
{
// One import edge: the derived scope is the caller, name the label, and
// libraryId/libraryVersionId the target library version.
class LibraryAsset : public LibraryAssetBase
{
public:
    bool decode(SimpleArray<uint8_t>&, Factory*) override { return true; }
    std::string fileExtension() const override { return "library"; }

protected:
    // Pure data, nothing for asset resolution to load.
    bool addsToBackboard() override { return false; }
};
} // namespace rive

#endif
