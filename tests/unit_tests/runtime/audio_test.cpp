#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_reader.hpp"
#include "rive/audio_event.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive_file_reader.hpp"
#include "catch.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/lua/scripting_vm.hpp"
#ifdef WITH_RIVE_SCRIPTING_LUAU
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace rive;

TEST_CASE("audio engine initializes", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);
  REQUIRE(engine != nullptr);
}

namespace
{
using Block = std::pair<uint32_t, uint64_t>;

// Renders stereo silence and keeps each block's frames and start frame.
class BlockLog : public AudioRenderer
{
public:
  std::vector<Block> blocks;

  void render(float* out, uint32_t frames, uint64_t startFrame) override
  {
    std::fill(out, out + (size_t)frames * 2, 0.0f);
    blocks.emplace_back(frames, startFrame);
  }
};

// Attaches another renderer during its first block, as a processor's init
// thread can while the audio thread mixes.
class AttachesDuringMix : public BlockLog
{
public:
  AudioEngine* engine = nullptr;
  AudioRenderer* late = nullptr;
  AudioRendererNode* lateNode = nullptr;

  void render(float* out, uint32_t frames, uint64_t startFrame) override
  {
    BlockLog::render(out, frames, startFrame);
    if (lateNode == nullptr)
    {
      lateNode = engine->attachRenderer(late);
    }
  }
};

// Stops a sound during its first block, as a script can while the audio
// thread mixes.
class StopsDuringMix : public BlockLog
{
public:
  AudioSound* sound = nullptr;
  uint64_t fade = 0;

  void render(float* out, uint32_t frames, uint64_t startFrame) override
  {
    BlockLog::render(out, frames, startFrame);
    if (blocks.size() == 1)
    {
      sound->stop(fade);
    }
  }
};

rcp<AudioEngine> engineWithBuffer(uint32_t frames)
{
  AudioEngine::SetBufferFrames(frames);
  rcp<AudioEngine> engine = AudioEngine::Make(2, 48000);
  AudioEngine::SetBufferFrames(0);
  REQUIRE(engine != nullptr);
  return engine;
}

rcp<AudioSource> stereo(std::vector<float> samples)
{
  return rcp<AudioSource>(
      new AudioSource(Span<float>(samples.data(), samples.size()), 2, 48000));
}

// Stereo samples at full scale, so every frame a sound reaches reads 1.
rcp<AudioSource> fullScale(uint32_t frames)
{
  return stereo(std::vector<float>((size_t)frames * 2, 1.0f));
}

// Each frame reads its own level, so a frame out of place reads wrong.
rcp<AudioSource> ramp(uint32_t frames)
{
  std::vector<float> samples((size_t)frames * 2);
  for (size_t i = 0; i < samples.size(); i++)
  {
    samples[i] = (float)(i / 2 + 1) / frames;
  }
  return stereo(std::move(samples));
}

std::vector<float> mix(AudioEngine* engine, uint32_t frames)
{
  std::vector<float> out((size_t)frames * 2);
  REQUIRE(engine->readAudioFrames(out.data(), frames));
  return out;
}

int64_t firstHeard(const std::vector<float>& stereo)
{
  for (size_t i = 0; i < stereo.size(); i += 2)
  {
    if (stereo[i] != 0.0f)
    {
      return (int64_t)(i / 2);
    }
  }
  return -1;
}

int64_t lastHeard(const std::vector<float>& stereo)
{
  for (size_t i = stereo.size(); i > 0; i -= 2)
  {
    if (stereo[i - 2] != 0.0f)
    {
      return (int64_t)(i / 2 - 1);
    }
  }
  return -1;
}
} // namespace

TEST_CASE("an engine mixes in blocks of the buffer it opens with", "[audio]")
{
  rcp<AudioEngine> engine = engineWithBuffer(2048);
  CHECK(engine->bufferFrames() == 2048);
  BlockLog log;
  AudioRendererNode* node = engine->attachRenderer(&log);
  REQUIRE(node != nullptr);
  // Pulls of fewer frames than a block still mix whole blocks.
  std::vector<float> frames(800 * 2);
  for (int pull = 0; pull < 4; pull++)
  {
    REQUIRE(engine->readAudioFrames(frames.data(), 800));
  }
  CHECK(log.blocks == std::vector<Block>{{2048, 0}, {2048, 2048}});
  engine->detachRenderer(node);
}

TEST_CASE(
    "the runtime engine opens with the buffer set before it is made",
    "[audio]")
{
  AudioEngine::ReleaseRuntimeEngine();
  AudioEngine::SetBufferFrames(4096);
  rcp<AudioEngine> made = AudioEngine::RuntimeEngine();
  REQUIRE(made != nullptr);
  CHECK(made->bufferFrames() == 4096);
  rcp<AudioEngine> stored = AudioEngine::MakeAndStore(2, 44100);
  REQUIRE(stored != nullptr);
  CHECK(stored->bufferFrames() == 4096);
  AudioEngine::SetBufferFrames(0);
  CHECK(AudioEngine::RuntimeEngine()->bufferFrames() == 4096);
  AudioEngine::ReleaseRuntimeEngine();
  CHECK(AudioEngine::RuntimeEngine()->bufferFrames() == 128);
  AudioEngine::ReleaseRuntimeEngine();
}

TEST_CASE(
    "an engine with no device runs at the default rate unless given one",
    "[audio]")
{
  const uint32_t defaultRate = AudioEngine::defaultSampleRate;
  rcp<AudioEngine> engine = AudioEngine::Make(2, 0);
  REQUIRE(engine != nullptr);
  CHECK(engine->sampleRate() == defaultRate);
  rcp<AudioEngine> given = AudioEngine::Make(2, 44100);
  REQUIRE(given != nullptr);
  CHECK(given->sampleRate() == 44100);
  // The recorder encodes what its runtime engine mixes at the default rate.
  AudioEngine::ReleaseRuntimeEngine();
  CHECK(AudioEngine::RuntimeEngine()->sampleRate() == defaultRate);
  rcp<AudioEngine> stored = AudioEngine::MakeAndStore(2, 0);
  REQUIRE(stored != nullptr);
  CHECK(stored->sampleRate() == defaultRate);
  AudioEngine::ReleaseRuntimeEngine();
}

TEST_CASE(
    "a renderer attached during a mix starts on the engine's clock",
    "[audio]")
{
  rcp<AudioEngine> engine = engineWithBuffer(4096);
  BlockLog late;
  AttachesDuringMix first;
  first.engine = engine.get();
  first.late = &late;
  AudioRendererNode* node = engine->attachRenderer(&first);
  REQUIRE(node != nullptr);
  std::vector<float> frames(4096 * 2);
  REQUIRE(engine->readAudioFrames(frames.data(), 4096));
  REQUIRE(engine->readAudioFrames(frames.data(), 4096));
  REQUIRE(first.lateNode != nullptr);
  CHECK(first.blocks == std::vector<Block>{{4096, 0}, {4096, 4096}});
  CHECK(late.blocks == std::vector<Block>{{4096, 4096}});
  engine->detachRenderer(first.lateNode);
  engine->detachRenderer(node);
}

TEST_CASE(
    "an engine starts a scheduled sound on its frame inside a block",
    "[audio]")
{
  for (uint64_t frame : {1000u, 5000u})
  {
    rcp<AudioEngine> engine = engineWithBuffer(4096);
    REQUIRE(engine->play(fullScale(48000), frame, 0, 0) != nullptr);
    CHECK(firstHeard(mix(engine.get(), 2 * 4096)) == (int64_t)frame);
  }
  // As a float, 0.251 falls just short of 251 milliseconds.
  rcp<AudioEngine> timed = engineWithBuffer(4096);
  REQUIRE(timed->playSeconds(fullScale(48000), 0.251f, 0, 0) != nullptr);
  CHECK(firstHeard(mix(timed.get(), 3 * 4096)) == 12048);
  // A sound played at a frame already mixed starts with the next block, and
  // the clock only runs while something is attached.
  rcp<AudioEngine> late = engineWithBuffer(4096);
  BlockLog clock;
  AudioRendererNode* node = late->attachRenderer(&clock);
  REQUIRE(node != nullptr);
  mix(late.get(), 4096);
  REQUIRE(late->timeInFrames() == 4096);
  REQUIRE(late->play(fullScale(48000), 100, 0, 0) != nullptr);
  CHECK(firstHeard(mix(late.get(), 4096)) == 0);
  late->detachRenderer(node);
}

TEST_CASE(
    "an engine fades a sound out to silence over its fade frames",
    "[audio]")
{
  rcp<AudioEngine> engine = engineWithBuffer(4096);
  rcp<AudioSound> sound = engine->play(fullScale(48000), 0, 0, 0);
  REQUIRE(sound != nullptr);
  mix(engine.get(), 4096);
  sound->stop(1000);
  std::vector<float> out = mix(engine.get(), 4096);
  int wrong = 0;
  for (size_t i = 0; i < out.size(); i++)
  {
    size_t frame = i / 2;
    float want = frame < 1000 ? 1.0f - frame / 1000.0f : 0.0f;
    wrong += std::abs(out[i] - want) > 1e-6f;
  }
  CHECK(wrong == 0);
}

TEST_CASE("a sound stopped with a fade before it starts never plays", "[audio]")
{
  for (uint64_t fade : {1000u, 4000u})
  {
    rcp<AudioEngine> engine = engineWithBuffer(4096);
    rcp<AudioSound> sound = engine->play(fullScale(2000), 3000, 0, 0);
    REQUIRE(sound != nullptr);
    sound->stop(fade);
    // A whole block mixes straight into out, so a write past the block
    // lands in its second half.
    std::vector<float> out(2 * 4096 * 2, 0.5f);
    REQUIRE(engine->readAudioFrames(out.data(), 4096));
    CHECK(std::count(out.begin() + 4096 * 2, out.end(), 0.5f) == 4096 * 2);
    std::vector<float> heard(out.begin(), out.begin() + 4096 * 2);
    std::vector<float> rest = mix(engine.get(), 2 * 4096);
    heard.insert(heard.end(), rest.begin(), rest.end());
    CHECK(firstHeard(heard) == -1);
    CHECK_FALSE(sound->completed());
    CHECK(engine->playingSoundCount() == 1);
  }
}

TEST_CASE("a sound stopped during the block it starts in fades out", "[audio]")
{
  rcp<AudioEngine> engine = engineWithBuffer(4096);
  // Attached before the sound, so it renders after the sound mixed.
  StopsDuringMix stopper;
  AudioRendererNode* node = engine->attachRenderer(&stopper);
  REQUIRE(node != nullptr);
  rcp<AudioSound> sound = engine->play(fullScale(48000), 1000, 0, 0);
  REQUIRE(sound != nullptr);
  stopper.sound = sound.get();
  stopper.fade = 8000;
  std::vector<float> out = mix(engine.get(), 3 * 4096);
  CHECK(firstHeard(out) == 1000);
  CHECK(lastHeard(out) == 7999);
  engine->detachRenderer(node);
}

TEST_CASE("a sound that seeks plays none of what it read ahead", "[audio]")
{
  rcp<AudioEngine> engine = engineWithBuffer(128);
  // Starting inside a block leaves frames read ahead past it.
  rcp<AudioSound> sound = engine->play(ramp(200000), 50, 0, 0);
  REQUIRE(sound != nullptr);
  mix(engine.get(), 1280);
  REQUIRE(sound->seek(0));
  std::vector<float> out = mix(engine.get(), 128);
  CHECK(out[0] == 1.0f / 200000);
  CHECK(out[2] == 2.0f / 200000);
}

TEST_CASE("a scheduled sound completes in the block it ends in", "[audio]")
{
  rcp<AudioEngine> engine = engineWithBuffer(4096);
  rcp<AudioSound> sound = engine->play(fullScale(1000), 1000, 0, 0);
  REQUIRE(sound != nullptr);
  mix(engine.get(), 4096);
  CHECK(sound->completed());
  CHECK(engine->playingSoundCount() == 0);
}

TEST_CASE(
    "an engine with no device starts a sound on its frame under a newer one",
    "[audio]")
{
  const uint32_t length = 4096;
  for (bool monitored : {false, true})
  {
    for (uint32_t read : {1000u, 2048u, 4096u})
    {
      for (uint64_t start : {1u, 479u, 480u, 1000u, 2047u})
      {
        INFO(
            "monitored " << monitored << ", reads of " << read << ", start "
                         << start);
        rcp<AudioEngine> engine = AudioEngine::Make(2, 48000);
        REQUIRE(engine != nullptr);
        if (monitored)
        {
          engine->initLevelMonitor();
        }
        REQUIRE(engine->play(ramp(length), start, 0, 0) != nullptr);
        REQUIRE(
            engine->play(stereo(std::vector<float>(length * 2)), 0, 0, 0) !=
            nullptr);
        std::vector<float> heard;
        while (heard.size() < (start + length) * 2)
        {
          std::vector<float> out = mix(engine.get(), read);
          heard.insert(heard.end(), out.begin(), out.end());
        }
        CHECK(firstHeard(heard) == (int64_t)start);
        int wrong = 0;
        for (size_t i = 0; i < (start + length) * 2; i++)
        {
          size_t frame = i / 2;
          float want = frame < start ? 0.0f
                                     : (float)(frame - start + 1) / length;
          wrong += std::abs(heard[i] - want) > 1e-6f;
        }
        CHECK(wrong == 0);
        CHECK(engine->playingSoundCount() == 0);
      }
    }
  }
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
  auto file = loadFile("assets/audio/what.wav");
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
    // The frames that decode.
    REQUIRE(reader->lengthInFrames() == 10545);
  }
  {
    auto reader = audioSource->makeReader(2, 32000);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->lengthInFrames() == 7030);
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
  auto file = ReadRiveFile("assets/sound.riv");
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
    auto file = loadFile("assets/audio/what.wav");
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
    auto file = loadFile("assets/audio/what.wav");
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

  auto file = ReadRiveFile("assets/sound.riv");
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

// An artboard with no engine of its own still starts its sounds on the
// runtime engine, because AudioEvent::play falls back to it. Teardown has to
// resolve the engine the same way, or those sounds keep playing into whatever
// the process renders next. Artboard::audioEngine only pushes down the tree at
// the moment it is called, so artboards instantiated later -- list items from
// ArtboardComponentList::createArtboard -- still reach teardown with none.
TEST_CASE(
    "audio sounds stop when the artboard has no engine assigned",
    "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);
  REQUIRE(AudioEngine::RuntimeEngine(false) == engine);

  auto file = ReadRiveFile("assets/sound.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  // Deliberately no artboard->audioEngine(engine) here.

  auto audioEvent = artboard->find<AudioEvent>()[0];
  REQUIRE(audioEvent->asset() != nullptr);
  REQUIRE(audioEvent->asset()->hasAudioSource());

  audioEvent->play();
  audioEvent->play();
  REQUIRE(engine->playingSoundCount() == 2);

  artboard = nullptr;
  REQUIRE(engine->playingSoundCount() == 0);
}

TEST_CASE("Artboard has audio", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);

  auto file = ReadRiveFile("assets/sound2.riv");
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

  auto file = ReadRiveFile("assets/sound2.riv");
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

  auto file = ReadRiveFile("assets/sound2.riv");
  auto artboard = file->artboardNamed("no-audio");
  artboard->audioEngine(engine);

  REQUIRE(artboard != nullptr);

  auto audioEvents = artboard->find<AudioEvent>();
  REQUIRE(audioEvents.size() == 0);
  REQUIRE(artboard->hasAudio() == false);
}

// List items are created after the engine is set, when the list's view model
// fills in, so they have to pick it up from their host.
TEST_CASE("List items use their host's audio engine", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);

  auto file = ReadRiveFile("assets/component_list_1.riv");
  auto artboard = file->artboard("Main")->instance();
  REQUIRE(artboard != nullptr);
  artboard->audioEngine(engine);

  auto list = artboard->find<ArtboardComponentList>("List");
  REQUIRE(list != nullptr);
  REQUIRE(list->artboardCount() == 0);

  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  REQUIRE(viewModelInstance != nullptr);
  artboard->bindViewModelInstance(viewModelInstance);
  artboard->advance(0.0f);

  REQUIRE(list->artboardCount() > 0);
  for (int i = 0; i < (int)list->artboardCount(); i++)
  {
    auto item = list->artboardInstance(i);
    REQUIRE(item != nullptr);
    CHECK(item->audioEngine() == engine);
  }
}

// Same for a nested artboard swapped in by data binding after the engine is
// set.
TEST_CASE("Swapped nested artboards use their host's audio engine", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::Make(2, 44100);

  auto file = ReadRiveFile("assets/data_binding_artboards_test.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  artboard->audioEngine(engine);

  auto stateMachine = artboard->stateMachineAt(0);
  int viewModelId = artboard.get()->viewModelId();
  auto vmi = viewModelId == -1 ? file->createViewModelInstance(artboard.get())
                               : file->createViewModelInstance(viewModelId, 0);
  stateMachine->bindViewModelInstance(vmi);
  stateMachine->advanceAndApply(0.1f);

  auto nestedArtboards = artboard->find<NestedArtboard>();
  std::vector<ArtboardInstance*> before;
  for (auto nested : nestedArtboards)
  {
    before.push_back(nested->artboardInstance());
  }

  auto vmiArtboard = vmi->propertyValue("ab")->as<ViewModelInstanceArtboard>();
  vmiArtboard->asset(file->bindableArtboardNamed("ch1"));
  stateMachine->advanceAndApply(0.1f);

  bool swapped = false;
  for (size_t i = 0; i < nestedArtboards.size(); i++)
  {
    auto instance = nestedArtboards[i]->artboardInstance();
    if (instance == nullptr)
    {
      continue;
    }
    swapped = swapped || instance != before[i];
    CHECK(instance->audioEngine() == engine);
  }
  REQUIRE(swapped);
}

// Scripted audio drives a Luau VM; the wasm backend covers audio through
// the differential harness instead.
#ifdef WITH_RIVE_SCRIPTING_LUAU
TEST_CASE("Scripted audio plays", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/audio_script.riv");
  auto artboard = file->artboardDefault();
  artboard->audioEngine(engine);
  auto vm = file->scriptingVM();
  auto context = vm->context();
  context->isPlaying(true);

  auto stateMachine = artboard->stateMachineAt(0);
  auto vmi = file->createDefaultViewModelInstance(artboard.get());

  stateMachine->bindViewModelInstance(vmi);

  stateMachine->advanceAndApply(0.016f);

  REQUIRE(engine->playingSoundCount() == 0);

  stateMachine->pointerDown(rive::Vec2D(25.0f, 25.0f));
  stateMachine->pointerUp(rive::Vec2D(25.0f, 25.0f));

  stateMachine->advanceAndApply(0.016f);

  REQUIRE(artboard != nullptr);

  REQUIRE(engine->playingSoundCount() == 1);

  auto sound = engine->playingSoundsHead();
  REQUIRE(sound->volume() == 1.0f);

  stateMachine->pointerDown(rive::Vec2D(200.0f, 200.0f));
  stateMachine->pointerUp(rive::Vec2D(200.0f, 200.0f));
  stateMachine->advanceAndApply(0.016f);

  REQUIRE(sound->volume() == 0.1f);
}
#endif

TEST_CASE("audio source duration from file", "[audio]")
{
  auto file = loadFile("assets/audio/what.wav");
  auto span = Span<uint8_t>(file);
  rcp<AudioSource> audioSource = rcp<AudioSource>(new AudioSource(span));
  REQUIRE(audioSource->channels() == 2);
  REQUIRE(audioSource->sampleRate() == 44100);

  float duration = audioSource->duration();
  // 9688 frames at 44100 Hz
  REQUIRE(duration == Approx(9688.0f / 44100.0f));

  // Verify caching: second call returns same value without re-decoding.
  REQUIRE(audioSource->duration() == duration);
}

TEST_CASE("audio source duration from buffered samples", "[audio]")
{
  const uint32_t numChannels = 2;
  const uint32_t sampleRate = 48000;
  const uint32_t numFrames = 48000; // 1 second worth of frames
  std::vector<float> samples(numFrames * numChannels, 0.0f);
  auto span = Span<float>(samples.data(), samples.size());
  rcp<AudioSource> audioSource = rcp<AudioSource>(
      new AudioSource(span, numChannels, sampleRate));
  REQUIRE(audioSource->isBuffered());
  REQUIRE(audioSource->duration() == Approx(1.0f));
}

#ifndef MA_NO_MP3
TEST_CASE("audio source duration mp3", "[audio]")
{
  auto file = loadFile("assets/audio/song.mp3");
  auto span = Span<uint8_t>(file);
  rcp<AudioSource> audioSource = rcp<AudioSource>(new AudioSource(span));

  float duration = audioSource->duration();
  // Duration should be positive for a valid mp3.
  REQUIRE(duration > 0.0f);
  // Cached value should match.
  REQUIRE(audioSource->duration() == duration);
}
#endif

#ifndef MA_NO_MP3
// Our miniaudio carries dr_libs' seek table fixes, which its releases lacked.
TEST_CASE("an mp3 reader seeks to the frames a whole decode has", "[audio]")
{
  auto file = loadFile("assets/audio/song.mp3");
  rcp<AudioSource> audioSource = rcp<AudioSource>(
      new AudioSource(Span<uint8_t>(file)));
  std::vector<float> whole;
  REQUIRE(audioSource->makeReader(0, 0)->readAll(UINT64_MAX, whole));
  auto reader = audioSource->makeReader(0, 0);
  REQUIRE(reader != nullptr);
  uint32_t channels = reader->channels();
  uint64_t total = whole.size() / channels;
  REQUIRE(total > 100000);
  // Backwards too, and before the first seek point.
  for (uint64_t frame : {total * 3 / 4, total / 3, (uint64_t)12345})
  {
    INFO("frame " << frame);
    REQUIRE(reader->seek(frame));
    std::vector<float> read(4096 * channels);
    uint64_t framesRead = 0;
    REQUIRE(reader->readInto(read.data(), 4096, framesRead));
    REQUIRE(framesRead == 4096);
    for (size_t i = 0; i < read.size(); i++)
    {
      REQUIRE(read[i] == whole[frame * channels + i]);
    }
  }
  // Just past where a first read leaves the decoder, in frames that count
  // its encoder delay of 1105.
  {
    auto fresh = audioSource->makeReader(0, 0);
    std::vector<float> first(4096 * channels);
    uint64_t framesRead = 0;
    REQUIRE(fresh->readInto(first.data(), 4096, framesRead));
    REQUIRE(fresh->seek(4096 + 1105));
    REQUIRE(fresh->readInto(first.data(), 4096, framesRead));
    REQUIRE(framesRead == 4096);
    for (size_t i = 0; i < first.size(); i++)
    {
      REQUIRE(first[i] == whole[(4096 + 1105) * channels + i]);
    }
  }
  // Past the end it lands at the end, with nothing more to read.
  uint64_t landed = 0;
  REQUIRE(reader->seek(total + 1000, &landed));
  REQUIRE(landed == total);
  std::vector<float> tail(16 * channels);
  uint64_t framesRead = 1;
  REQUIRE(reader->readInto(tail.data(), 16, framesRead));
  REQUIRE(framesRead == 0);
}
#endif

TEST_CASE("a resampled reader filters as a resampling decoder does", "[audio]")
{
  auto file = loadFile("assets/audio/what.wav");
  rcp<AudioSource> audioSource = rcp<AudioSource>(
      new AudioSource(Span<uint8_t>(file)));
  ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 16000);
  ma_decoder decoder;
  REQUIRE(
      ma_decoder_init_memory(file.data(), file.size(), &config, &decoder) ==
      MA_SUCCESS);
  std::vector<float> expected(2048);
  ma_uint64 decoded = 0;
  ma_decoder_read_pcm_frames(&decoder, expected.data(), 2048, &decoded);
  ma_decoder_uninit(&decoder);
  auto reader = audioSource->makeReader(1, 16000);
  std::vector<float> read(2048);
  uint64_t framesRead = 0;
  REQUIRE(reader->readInto(read.data(), 2048, framesRead));
  REQUIRE(framesRead == decoded);
  for (size_t i = 0; i < read.size(); i++)
  {
    INFO("frame " << i);
    REQUIRE(read[i] == Approx(expected[i]).margin(1e-6));
  }
}

TEST_CASE("an audio decode stops past its cap or when asked", "[audio]")
{
  auto file = loadFile("assets/audio/what.wav");
  rcp<AudioSource> audioSource = rcp<AudioSource>(
      new AudioSource(Span<uint8_t>(file)));
  std::vector<float> samples;
  REQUIRE(audioSource->makeReader(1, 0)->readAll(1000, samples));
  // It stops at the first chunk past the cap, which the caller refuses.
  REQUIRE(samples.size() > 1000);
  REQUIRE(samples.size() <= 1000 + 4096);
  std::atomic<bool> stop{true};
  REQUIRE_FALSE(
      audioSource->makeReader(1, 0)->readAll(UINT64_MAX, samples, &stop));
  REQUIRE(samples.empty());
}

// TODO check if sound->stop calls completed callback!!!