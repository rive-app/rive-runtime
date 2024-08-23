#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_reader.hpp"
#include "rive/audio_event.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive_file_reader.hpp"
#include "catch.hpp"
#include <string>

using namespace rive;

TEST_CASE("audio engine initializes", "[audio]")
{
    rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);
    REQUIRE(engine != nullptr);
}

static std::vector<uint8_t> loadFile(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    REQUIRE(fp != nullptr);

    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    REQUIRE(fread(bytes.data(), 1, length, fp) == length);
    fclose(fp);

    return bytes;
}

TEST_CASE("audio source can be opened", "[audio]")
{
    rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);
    REQUIRE(engine != nullptr);
    auto file = loadFile("../../test/assets/audio/what.wav");
    auto span = Span<uint8_t>(file);
    rcp<AudioSource> audioSource = rcp<AudioSource>(new AudioSource(span));
    REQUIRE(audioSource->channels() == 2);
    REQUIRE(audioSource->sampleRate() == 44100);

    // Try some different sample rates.
    {
        auto reader = audioSource->makeReader(2, 44100);
        REQUIRE(reader != nullptr);
        REQUIRE(reader->lengthInFrames() == 9688);
    }
    {
        auto reader = audioSource->makeReader(1, 48000);
        REQUIRE(reader != nullptr);
        REQUIRE(reader->lengthInFrames() == 10544);
    }
    {
        auto reader = audioSource->makeReader(2, 32000);
        REQUIRE(reader != nullptr);
        REQUIRE(reader->lengthInFrames() == 7029);
    }

    float channels[2] = {0, 0};
    engine->initLevelMonitor();
    engine->levels(Span<float>(&channels[0], 2));
    REQUIRE(channels[0] == 0);
    REQUIRE(channels[1] == 0);

    auto sound = engine->play(audioSource, 0, 0, 0);
    float frames[512 * 2] = {};
    engine->readAudioFrames(frames, 512);
    engine->levels(Span<float>(&channels[0], 2));
    REQUIRE(channels[0] != 0);
    REQUIRE(channels[1] != 0);

    engine->readAudioFrames(frames, 512);
    REQUIRE(engine->level(0) != 0);
    REQUIRE(engine->level(1) != 0);
}

TEST_CASE("file with audio loads correctly", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/sound.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);

    auto audioEvents = artboard->find<AudioEvent>();
    REQUIRE(audioEvents.size() == 1);

    auto audioEvent = audioEvents[0];
    REQUIRE(audioEvent->asset() != nullptr);
    REQUIRE(audioEvent->asset()->hasAudioSource());

    // auto textObjects = artboard->find<rive::Text>();
    // REQUIRE(textObjects.size() == 5);

    // auto styleObjects = artboard->find<rive::TextStyle>();
    // REQUIRE(styleObjects.size() == 13);

    // auto runObjects = artboard->find<rive::TextValueRun>();
    // REQUIRE(runObjects.size() == 22);

    // artboard->advance(0.0f);
    // rive::NoOpRenderer renderer;
    // artboard->draw(&renderer);
}

TEST_CASE("audio sound can outlive engine", "[audio]")
{
    rcp<AudioSound> sound;
    {
        rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);
        REQUIRE(engine != nullptr);
        auto file = loadFile("../../test/assets/audio/what.wav");
        auto span = Span<uint8_t>(file);
        rcp<AudioSource> audioSource = rcp<AudioSource>(new AudioSource(span));
        REQUIRE(audioSource->channels() == 2);
        REQUIRE(audioSource->sampleRate() == 44100);

        sound = engine->play(audioSource, 0, 0, 0);
        float frames[512 * 2] = {};
        engine->readAudioFrames(frames, 512);
    }
    sound->stop();
}

TEST_CASE("many audio sounds can outlive engine", "[audio]")
{
    std::vector<rcp<AudioSound>> sounds;
    {
        rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);
        REQUIRE(engine != nullptr);
        auto file = loadFile("../../test/assets/audio/what.wav");
        auto span = Span<uint8_t>(file);
        rcp<AudioSource> audioSource = rcp<AudioSource>(new AudioSource(span));
        REQUIRE(audioSource->channels() == 2);
        REQUIRE(audioSource->sampleRate() == 44100);

        for (int i = 0; i < 20; i++)
        {
            sounds.emplace_back(engine->play(audioSource, 0, 0, 0));
        }
        float frames[512 * 2] = {};
        engine->readAudioFrames(frames, 512);
    }
    for (auto sound : sounds)
    {
        sound->stop();
    }
}

TEST_CASE("audio sounds from different artboards stop accordingly", "[audio]")
{
    rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);

    auto file = ReadRiveFile("../../test/assets/sound.riv");
    auto artboard = file->artboardDefault();
    artboard->audioEngine(engine);
    auto artboard2 = file->artboardDefault();
    artboard2->audioEngine(engine);

    REQUIRE(artboard != nullptr);

    auto audioEvents = artboard->find<AudioEvent>();
    REQUIRE(audioEvents.size() == 1);

    auto audioEvent = audioEvents[0];
    REQUIRE(audioEvent->asset() != nullptr);
    REQUIRE(audioEvent->asset()->hasAudioSource());

    audioEvent->play();
    audioEvent->play();
    REQUIRE(engine->playingSoundCount() == 2);
    auto audioEvent2 = artboard2->find<AudioEvent>()[0];
    audioEvent2->play();
    REQUIRE(engine->playingSoundCount() == 3);
    audioEvent->play();
    REQUIRE(engine->playingSoundCount() == 4);

    // The three playing sounds owned by the first artboard should now stop.
    artboard = nullptr;

    REQUIRE(engine->playingSoundCount() == 1);

    // The last one belonging to artboard2 should now stop too.
    artboard2 = nullptr;
    REQUIRE(engine->playingSoundCount() == 0);
}

TEST_CASE("Artboard has audio", "[audio]")
{
    rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);

    auto file = ReadRiveFile("../../test/assets/sound2.riv");
    auto artboard = file->artboardNamed("child");
    artboard->audioEngine(engine);

    REQUIRE(artboard != nullptr);

    auto audioEvents = artboard->find<AudioEvent>();
    REQUIRE(audioEvents.size() == 1);
    REQUIRE(artboard->hasAudio() == true);
}

TEST_CASE("Artboard has audio in nested artboard", "[audio]")
{
    rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);

    auto file = ReadRiveFile("../../test/assets/sound2.riv");
    auto artboard = file->artboardNamed("grand-parent");
    artboard->audioEngine(engine);

    REQUIRE(artboard != nullptr);

    auto audioEvents = artboard->find<AudioEvent>();
    REQUIRE(audioEvents.size() == 0);
    REQUIRE(artboard->hasAudio() == true);
}

TEST_CASE("Artboard does not have audio", "[audio]")
{
    rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);

    auto file = ReadRiveFile("../../test/assets/sound2.riv");
    auto artboard = file->artboardNamed("no-audio");
    artboard->audioEngine(engine);

    REQUIRE(artboard != nullptr);

    auto audioEvents = artboard->find<AudioEvent>();
    REQUIRE(audioEvents.size() == 0);
    REQUIRE(artboard->hasAudio() == false);
}

// TODO check if sound->stop calls completed callback!!!