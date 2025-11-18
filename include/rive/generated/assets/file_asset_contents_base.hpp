#ifndef _RIVE_FILE_ASSET_CONTENTS_BASE_HPP_
#define _RIVE_FILE_ASSET_CONTENTS_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/span.hpp"
namespace rive
{
class FileAssetContentsBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 106;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FileAssetContentsBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t bytesPropertyKey = 212;
    static const uint16_t signaturePropertyKey = 911;

public:
    virtual void decodeBytes(Span<const uint8_t> value) = 0;
    virtual void copyBytes(const FileAssetContentsBase& object) = 0;

    virtual void decodeSignature(Span<const uint8_t> value) = 0;
    virtual void copySignature(const FileAssetContentsBase& object) = 0;

    Core* clone() const override;
    void copy(const FileAssetContentsBase& object)
    {
        copyBytes(object);
        copySignature(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case bytesPropertyKey:
                decodeBytes(CoreBytesType::deserialize(reader));
                return true;
            case signaturePropertyKey:
                decodeSignature(CoreBytesType::deserialize(reader));
                return true;
        }
        return false;
    }

protected:
    virtual void bytesChanged() {}
    virtual void signatureChanged() {}
};
} // namespace rive

#endif