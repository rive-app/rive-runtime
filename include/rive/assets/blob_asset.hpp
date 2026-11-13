#ifndef _RIVE_BLOB_ASSET_HPP_
#define _RIVE_BLOB_ASSET_HPP_
#include "rive/generated/assets/blob_asset_base.hpp"
#include "rive/simple_array.hpp"

namespace rive
{
class BlobAsset : public BlobAssetBase
{
public:
  BlobAsset();
  bool decode(SimpleArray<uint8_t>&, Factory*) override;
  std::string fileExtension() const override;

  Span<const uint8_t> bytes() const { return m_bytes; }
  // New with the blob and with each decode, and never zero, so the holder
  // of a copy of the bytes can tell when it is stale.
  uint32_t bytesId() const { return m_bytesId; }

private:
  SimpleArray<uint8_t> m_bytes;
  uint32_t m_bytesId;
};
} // namespace rive

#endif