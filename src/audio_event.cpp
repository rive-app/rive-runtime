#include "rive/audio_event.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/artboard.hpp"

using namespace rive;

void AudioEvent::play()
{
#ifdef WITH_RIVE_AUDIO
    auto audioAsset = (AudioAsset*)m_fileAsset;
    if (audioAsset == nullptr)
    {
        return;
    }
    auto audioSource = audioAsset->audioSource();
    if (audioSource == nullptr)
    {
        return;
    }

    auto volume = audioAsset->volume() * artboard()->volume();
    if (volume <= 0.0f)
    {
        return;
    }

    auto engine =
#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
        artboard()->audioEngine() != nullptr ? artboard()->audioEngine() :
#endif
                                             AudioEngine::RuntimeEngine();

    auto sound = engine->play(audioSource, engine->timeInFrames(), 0, 0, artboard());

    if (volume != 1.0f)
    {
        sound->volume(volume);
    }
#endif
}

void AudioEvent::trigger(const CallbackData& value)
{
    Super::trigger(value);
    if (!value.context()->playsAudio())
    {
        // Context won't play audio, we'll do it ourselves.
        play();
    }
}

StatusCode AudioEvent::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

void AudioEvent::setAsset(FileAsset* asset)
{
    if (asset->is<AudioAsset>())
    {
        FileAssetReferencer::setAsset(asset);
    }
}

Core* AudioEvent::clone() const
{
    AudioEvent* twin = AudioEventBase::clone()->as<AudioEvent>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

uint32_t AudioEvent::assetId() { return AudioEventBase::assetId(); }