#include "rive/assets/blob_asset.hpp"

using namespace rive;

bool BlobAsset::decode(SimpleArray<uint8_t>& data, Factory*)
{
    m_bytes = std::move(data);
    return true;
}

std::string BlobAsset::fileExtension() const { return "blob"; }
