#ifndef _RIVE_VIEW_MODEL_INSTANCE_ASSET_IMAGE_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ASSET_IMAGE_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"

namespace rive
{

class ViewModelInstanceAssetImageRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceAssetImageRuntime(
        ViewModelInstanceAssetImage* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    void value(RenderImage* renderImage);
    const DataType dataType() override { return DataType::assetImage; }

#ifdef TESTING
    RenderImage* testing_value();
#endif
};
} // namespace rive
#endif
