#ifndef _RIVE_FOLDER_BASE_HPP_
#define _RIVE_FOLDER_BASE_HPP_
#include "rive/assets/asset.hpp"
namespace rive
{
class FolderBase : public Asset
{
protected:
    typedef Asset Super;

public:
    static const uint16_t typeKey = 102;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FolderBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif