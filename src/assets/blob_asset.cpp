#include "rive/assets/blob_asset.hpp"

#include <atomic>

using namespace rive;

static uint32_t nextBytesId()
{
  static std::atomic<uint32_t> next{1};
  uint32_t id = next.fetch_add(1, std::memory_order_relaxed);
  return id != 0 ? id : nextBytesId();
}

BlobAsset::BlobAsset() : m_bytesId(nextBytesId()) {}

bool BlobAsset::decode(SimpleArray<uint8_t>& data, Factory*)
{
  m_bytes = std::move(data);
  m_bytesId = nextBytesId();
  return true;
}

std::string BlobAsset::fileExtension() const { return "blob"; }
