#ifndef _RIVE_DATA_VALUE_ASSET_BLOB_HPP_
#define _RIVE_DATA_VALUE_ASSET_BLOB_HPP_
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include "rive/assets/blob_asset.hpp"
#include "rive/refcnt.hpp"

#include <iostream>
namespace rive
{
class DataValueAssetBlob : public DataValueInteger
{
public:
    DataValueAssetBlob(uint32_t value) : DataValueInteger(value) {};
    DataValueAssetBlob() : DataValueInteger(-1) {};
    static const DataType typeKey = DataType::assetBlob;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::assetBlob || typeKey == DataType::integer;
    };
    constexpr static uint32_t defaultValue = -1;
    rcp<BlobAsset> fileAsset() { return m_fileAsset; }
    void blobValue(BlobAsset* blob)
    {
        m_fileAsset = blob == nullptr ? nullptr : ref_rcp(blob);
    }
    BlobAsset* blobValue() { return m_fileAsset.get(); }

private:
    rcp<BlobAsset> m_fileAsset = nullptr;
};
} // namespace rive
#endif
