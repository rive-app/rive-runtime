#ifndef _RIVE_FILE_ASSET_CONTENTS_HPP_
#define _RIVE_FILE_ASSET_CONTENTS_HPP_
#include "rive/generated/assets/file_asset_contents_base.hpp"
#include <cstdint>
#include "rive/simple_array.hpp"

namespace rive
{
class FileAssetContents : public FileAssetContentsBase
{
public:
    SimpleArray<uint8_t>& bytes();
    StatusCode import(ImportStack& importStack) override;
    void decodeBytes(Span<const uint8_t> value) override;
    void copyBytes(const FileAssetContentsBase& object) override;

private:
    SimpleArray<uint8_t> m_bytes;
};
} // namespace rive

#endif
