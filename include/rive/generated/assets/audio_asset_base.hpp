#ifndef _RIVE_AUDIO_ASSET_BASE_HPP_
#define _RIVE_AUDIO_ASSET_BASE_HPP_
#include "rive/assets/export_audio.hpp"
namespace rive
{
class AudioAssetBase : public ExportAudio
{
protected:
    typedef ExportAudio Super;

public:
    static const uint16_t typeKey = 406;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case AudioAssetBase::typeKey:
            case ExportAudioBase::typeKey:
            case FileAssetBase::typeKey:
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