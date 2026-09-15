#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_ASSET_BLOB_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_ASSET_BLOB_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_asset_blob.hpp"
namespace rive
{
class BlobAsset;
class DataBindContextValueAssetBlob : public DataBindContextValue
{

public:
    DataBindContextValueAssetBlob(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection,
               DataBind* dataBind) override;
    rcp<BlobAsset> fileAsset(DataBind* dataBind);
};
} // namespace rive

#endif
