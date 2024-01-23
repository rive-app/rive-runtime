#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_source.hpp"
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
    AudioSource audioSource(span);
    REQUIRE(audioSource.channels() == 2);
    REQUIRE(audioSource.sampleRate() == 44100);

    // Try some different sample rates.
    {
        auto reader = audioSource.makeReader(2, 44100);
        REQUIRE(reader != nullptr);
        REQUIRE(reader->lengthInFrames() == 9688);
    }
    {
        auto reader = audioSource.makeReader(1, 48000);
        REQUIRE(reader != nullptr);
        REQUIRE(reader->lengthInFrames() == 10544);
    }
    {
        auto reader = audioSource.makeReader(2, 32000);
        REQUIRE(reader != nullptr);
        REQUIRE(reader->lengthInFrames() == 7029);
    }
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