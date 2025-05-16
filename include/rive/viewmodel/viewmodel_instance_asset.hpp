#ifndef _RIVE_VIEW_MODEL_INSTANCE_ASSET_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ASSET_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_asset_base.hpp"
#include "rive/assets/file_asset.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceAsset;
typedef void (*ViewModelAssetChanged)(ViewModelInstanceAsset* vmi,
                                      uint32_t value);
#endif
class ViewModelInstanceAsset : public ViewModelInstanceAssetBase
{
public:
    StatusCode import(ImportStack& importStack) override;
    void addAsset(FileAsset* asset) { m_assets.push_back(asset); }

protected:
    const std::vector<FileAsset*>& assets() const { return m_assets; }

private:
    std::vector<FileAsset*> m_assets;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(ViewModelAssetChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelAssetChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif