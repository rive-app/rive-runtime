#include "rive/assets/audio_asset.hpp"
#include "rive/factory.hpp"

using namespace rive;

AudioAsset::AudioAsset() {}

AudioAsset::~AudioAsset() {}

bool AudioAsset::decode(SimpleArray<uint8_t>& bytes, Factory* factory)
{
#ifdef WITH_RIVE_AUDIO
    // Steal the bytes.
    m_audioSource = rcp<AudioSource>(new AudioSource(std::move(bytes)));
#endif
    return true;
}

std::string AudioAsset::fileExtension() const { return "wav"; }