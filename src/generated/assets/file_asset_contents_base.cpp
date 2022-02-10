#include "rive/generated/assets/file_asset_contents_base.hpp"
#include "rive/assets/file_asset_contents.hpp"

using namespace rive;

Core* FileAssetContentsBase::clone() const
{
    auto cloned = new FileAssetContents();
    cloned->copy(*this);
    return cloned;
}
