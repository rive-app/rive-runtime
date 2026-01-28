#ifndef _RIVE_BLOB_ASSET_HPP_
#define _RIVE_BLOB_ASSET_HPP_
#include "rive/generated/assets/blob_asset_base.hpp"
#include "rive/simple_array.hpp"

namespace rive
{
class BlobAsset : public BlobAssetBase
{
public:
    bool decode(SimpleArray<uint8_t>&, Factory*) override;
    std::string fileExtension() const override;

    Span<const uint8_t> bytes() const { return m_bytes; }

private:
    SimpleArray<uint8_t> m_bytes;
};
} // namespace rive

#endif