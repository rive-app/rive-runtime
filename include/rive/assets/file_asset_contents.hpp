#ifndef _RIVE_FILE_ASSET_CONTENTS_HPP_
#define _RIVE_FILE_ASSET_CONTENTS_HPP_
#include "rive/generated/assets/file_asset_contents_base.hpp"
#include <cstdint>

namespace rive
{
class FileAssetContents : public FileAssetContentsBase
{
private:
    std::vector<uint8_t> m_Bytes;

public:
    Span<const uint8_t> bytes() const;
    StatusCode import(ImportStack& importStack) override;
    void decodeBytes(Span<const uint8_t> value) override;
    void copyBytes(const FileAssetContentsBase& object) override;
};
} // namespace rive

#endif
