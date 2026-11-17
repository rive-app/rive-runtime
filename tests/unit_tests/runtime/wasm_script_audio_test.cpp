#if defined(WITH_RIVE_AUDIO) && defined(WITH_RIVE_SCRIPTING_WASM) &&           \
    defined(WITH_RIVE_TOOLS)
#include "rive/artboard.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/file.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"
#include "rive_file_reader.hpp"
#include "kept_sound_test_helpers.hpp"
#include <catch.hpp>
#include <chrono>
#include <thread>

using namespace rive;

#ifndef __has_feature
#define __has_feature(x) 0
#endif

// ASan's larger frames overflow WAMR's native stack check at instantiate (as
// in script_signature_test), so there the module never runs.
#if !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)
// animascript_audio_owner.riv (source: assets/rml/animascript_audio_owner/,
// unsigned, so WITH_RIVE_TOOLS): a Node script plays a sound from init and
// another from its first advance, and keeps neither.
TEST_CASE(
    "A wasm script's sound stops when its artboard is deleted",
    "[audio][wasm]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/animascript_audio_owner.riv");
  auto* vm = file->wasmScriptingVM();
  REQUIRE(vm != nullptr);
  vm->isPlaying(true);

  auto artboard = file->artboardDefault();
  artboard->audioEngine(engine);
  REQUIRE(engine->playingSoundCount() == 1);
  artboard->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 2);

  artboard = nullptr;
  CHECK(engine->playingSoundCount() == 0);

  file = nullptr;
  CHECK(engine->playingSoundCount() == 0);
}

// Each artboard owns the sounds its own script instance starts.
TEST_CASE("A wasm script's sound belongs to its own artboard", "[audio][wasm]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/animascript_audio_owner.riv");
  REQUIRE(file->wasmScriptingVM() != nullptr);
  file->wasmScriptingVM()->isPlaying(true);

  auto first = file->artboardDefault();
  auto second = file->artboardDefault();
  first->advance(1.0f / 60.0f);
  second->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 4);

  first = nullptr;
  CHECK(engine->playingSoundCount() == 2);

  second = nullptr;
  CHECK(engine->playingSoundCount() == 0);
}
// Host's script makes its own instance of Main, whose script plays, and then
// plays itself: a hook that runs another script's hook gets its own caller
// back, so Host's sounds stop with Host and the instance's with the instance.
TEST_CASE(
    "A wasm script's sound after a nested script's hook is its own",
    "[audio][wasm]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/animascript_audio_owner.riv");
  REQUIRE(file->wasmScriptingVM() != nullptr);
  file->wasmScriptingVM()->isPlaying(true);

  // The deck input's instance, Host's own instance, then Host itself.
  auto host = file->artboardNamed("Host");
  REQUIRE(host != nullptr);
  REQUIRE(engine->playingSoundCount() == 3);
  host->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 5);

  // The file's VM still holds the instances.
  host = nullptr;
  CHECK(engine->playingSoundCount() == 3);

  file = nullptr;
  CHECK(engine->playingSoundCount() == 0);
}

// Setter sets a value in its advance, which runs Listener's listener at once;
// the sound Listener plays there is not Setter's.
TEST_CASE(
    "A wasm listener's sound is not the setting script's",
    "[audio][wasm]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/animascript_audio_owner.riv");
  REQUIRE(file->wasmScriptingVM() != nullptr);
  file->wasmScriptingVM()->isPlaying(true);

  auto setter = file->artboardNamed("Setter");
  auto listener = file->artboardNamed("Listener");
  REQUIRE(setter != nullptr);
  REQUIRE(listener != nullptr);
  auto game = file->createViewModelInstance("Game");
  REQUIRE(game != nullptr);
  setter->bindViewModelInstance(game);
  listener->bindViewModelInstance(game);

  for (int frame = 0; frame < 2; frame++)
  {
    listener->advance(1.0f / 60.0f);
    setter->advance(1.0f / 60.0f);
  }
  REQUIRE(engine->playingSoundCount() == 1);

  setter = nullptr;
  CHECK(engine->playingSoundCount() == 1);

  // The module knows the listener's owner but does not tell the host, so the
  // sound has no owner and outlives its artboard and file.
  listener = nullptr;
  CHECK(engine->playingSoundCount() == 1);
  file = nullptr;
  CHECK(engine->playingSoundCount() == 1);
}

// Decoder plays from the handler of a promise the host settles. Held outcomes
// are delivered at a poll outside any module call, so there is no caller.
TEST_CASE(
    "A wasm promise the host settles runs its handler with no owner",
    "[audio][wasm]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/animascript_audio_owner.riv");
  REQUIRE(file->wasmScriptingVM() != nullptr);
  file->wasmScriptingVM()->isPlaying(true);

  auto decoder = file->artboardNamed("Decoder");
  REQUIRE(decoder != nullptr);
  for (int frame = 0; frame < 1000 && engine->playingSoundCount() == 0; frame++)
  {
    decoder->advance(1.0f / 60.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(engine->playingSoundCount() == 1);

  decoder = nullptr;
  CHECK(engine->playingSoundCount() == 1);
  file = nullptr;
  CHECK(engine->playingSoundCount() == 1);
}

// Tuner plays from the setter of its level input, which the host sets outside
// any hook.
TEST_CASE(
    "A wasm input setter's sound stops when its artboard is deleted",
    "[audio][wasm]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/animascript_audio_owner.riv");
  REQUIRE(file->wasmScriptingVM() != nullptr);
  file->wasmScriptingVM()->isPlaying(true);

  auto tuner = file->artboardNamed("Tuner");
  REQUIRE(tuner != nullptr);
  auto* script = tuner->find<ScriptedDrawable>("Tuner");
  REQUIRE(script != nullptr);
  REQUIRE(engine->playingSoundCount() == 0);
  script->setNumberInput("level", 1.0f);
  REQUIRE(engine->playingSoundCount() == 1);

  tuner = nullptr;
  CHECK(engine->playingSoundCount() == 0);
}

// animascript_kept_sound.riv: another artboard's script can't restart,
// through a module both import, a sound whose artboard is deleted.
TEST_CASE(
    "A wasm script can't restart a sound whose artboard is deleted",
    "[audio][wasm]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/animascript_kept_sound.riv");
  REQUIRE(file->wasmScriptingVM() != nullptr);
  file->wasmScriptingVM()->isPlaying(true);
  kept_sound::checkOverForGood(file.get(), engine.get());
}
#endif
#endif
