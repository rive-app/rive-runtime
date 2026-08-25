#ifndef _RIVE_AUDIO_ASSET_BASE_HPP_
#define _RIVE_AUDIO_ASSET_BASE_HPP_
#include "rive/assets/export_audio.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class AudioAssetBase : public ExportAudio
{
protected:
    typedef ExportAudio Super;

public:
    static const uint16_t typeKey = 406;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

public:
    Core* clone() const override;
    void copy(const AudioAssetBase& object)
    {
        RIVE_EDITOR_COPY(object);
        ExportAudio::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ExportAudio::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/assets/audio_asset_ext.inl"
#endif
};
} // namespace rive

#endif