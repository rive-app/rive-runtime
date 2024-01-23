#ifndef _RIVE_AUDIO_ASSET_HPP_
#define _RIVE_AUDIO_ASSET_HPP_
#include "rive/generated/assets/audio_asset_base.hpp"
#include "rive/audio/audio_source.hpp"

namespace rive
{
class AudioAsset : public AudioAssetBase
{
public:
    AudioAsset();
    ~AudioAsset() override;
    bool decode(SimpleArray<uint8_t>&, Factory*) override;
    std::string fileExtension() const override;

#ifdef WITH_RIVE_AUDIO
#ifdef TESTING
    bool hasAudioSource() { return m_audioSource != nullptr; }
#endif

    rcp<AudioSource> audioSource() { return m_audioSource; }

private:
    rcp<AudioSource> m_audioSource;
#endif
};
} // namespace rive

#endif