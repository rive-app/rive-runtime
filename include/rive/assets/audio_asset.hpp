#ifndef _RIVE_AUDIO_ASSET_HPP_
#define _RIVE_AUDIO_ASSET_HPP_
#include "rive/generated/assets/audio_asset_base.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_engine.hpp"

namespace rive
{
class AudioAsset : public AudioAssetBase
{
public:
    AudioAsset();
    ~AudioAsset() override;
    bool decode(SimpleArray<uint8_t>&, Factory*) override;
    std::string fileExtension() const override;

#ifdef TESTING
    bool hasAudioSource() { return m_audioSource != nullptr; }
#endif

    rcp<AudioSource> audioSource() { return m_audioSource; }
    void audioSource(rcp<AudioSource> source) { m_audioSource = source; }
    void stop(rcp<AudioEngine> engine);

private:
    rcp<AudioSource> m_audioSource;
};
} // namespace rive

#endif