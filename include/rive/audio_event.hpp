#ifndef _RIVE_AUDIO_EVENT_HPP_
#define _RIVE_AUDIO_EVENT_HPP_
#include "rive/generated/audio_event_base.hpp"
#include "rive/assets/file_asset_referencer.hpp"

namespace rive
{
class AudioAsset;
class AudioEvent : public AudioEventBase, public FileAssetReferencer
{
public:
    StatusCode import(ImportStack& importStack) override;
    void setAsset(rcp<FileAsset> asset) override;
    uint32_t assetId() override;
    void trigger(const CallbackData& value) override;
    void play();

#ifdef TESTING
    AudioAsset* asset() const;
#endif
    Core* clone() const override;
};
} // namespace rive

#endif