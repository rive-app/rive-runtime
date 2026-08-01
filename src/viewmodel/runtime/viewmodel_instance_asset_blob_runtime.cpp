
#include "rive/viewmodel/runtime/viewmodel_instance_asset_blob_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

void ViewModelInstanceAssetBlobRuntime::value(BlobAsset* blob)
{
    m_viewModelInstanceValue->as<ViewModelInstanceAssetBlob>()->value(blob);
}

#ifdef TESTING
BlobAsset* ViewModelInstanceAssetBlobRuntime::testing_value()
{
    return m_viewModelInstanceValue->as<ViewModelInstanceAssetBlob>()
        ->asset()
        .get();
}
#endif
