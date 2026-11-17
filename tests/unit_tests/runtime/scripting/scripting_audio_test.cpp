#ifdef WITH_RIVE_AUDIO
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/artboard.hpp"
#include "rive/node.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_sound.hpp"
#include <cstdio>
#include <memory>
#include "rive/simple_array.hpp"
#include "rive/file.hpp"
#include "rive_file_reader.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "../kept_sound_test_helpers.hpp"

using namespace rive;

namespace
{
// A script that runs on a node of an artboard, as a Node script does.
class NodeScript : public ScriptedObject
{
public:
  explicit NodeScript(Component* component) : m_component(component) {}
  bool addScriptedDirt(ComponentDirt, bool) override { return true; }
  uint32_t assetId() override { return 0; }
  ScriptProtocol scriptProtocol() override { return ScriptProtocol::utility; }
  Component* component() override { return m_component; }

private:
  Component* m_component;
};

struct ScriptArtboard
{
  std::unique_ptr<Artboard> artboard;
  std::unique_ptr<NodeScript> script;

  explicit ScriptArtboard(Factory* factory)
      : artboard(std::make_unique<Artboard>(factory))
  {
    artboard->addObject(artboard.get());
    auto* node = new Node();
    artboard->addObject(node);
    node->parentId(0);
    REQUIRE(artboard->initialize() == StatusCode::Ok);
    REQUIRE(node->artboard() == artboard.get());
    script = std::make_unique<NodeScript>(node);
  }
};

rcp<AudioSource> loadSource(const char* filename)
{
  FILE* fp = fopen(filename, "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  const size_t length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  SimpleArray<uint8_t> bytes(length);
  REQUIRE(fread(bytes.data(), 1, length, fp) == length);
  fclose(fp);
  return rcp<AudioSource>(new AudioSource(std::move(bytes)));
}

// Calls the function on top of the stack with the source, as `script` would,
// and leaves its result on the stack.
void playFrom(lua_State* L, ScriptedObject* script, rcp<AudioSource> source)
{
  lua_pushvalue(L, -1);
  lua_newrive<ScriptedAudioSource>(L)->source(source);
  REQUIRE(rive_lua_pcall_with_context(L, script, 1, 1) == LUA_OK);
}
} // namespace

TEST_CASE(
    "A sound a script drops plays until its artboard is deleted",
    "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);
  ScriptingTest test("return function(source) return Audio.play(source) end");
#ifdef WITH_RIVE_TOOLS
  test.vm()->context()->isPlaying(true);
#endif
  lua_State* L = test.state();
  auto source = loadSource("assets/audio/what.wav");

  ScriptArtboard first(test.serializer());
  ScriptArtboard second(test.serializer());
  playFrom(L, first.script.get(), source);
  REQUIRE(lua_isuserdata(L, -1));
  lua_pop(L, 1);
  playFrom(L, second.script.get(), source);
  lua_pop(L, 1);

  lua_gc(L, LUA_GCCOLLECT, 0);
  CHECK(engine->playingSoundCount() == 2);

  first.script = nullptr;
  first.artboard = nullptr;
  CHECK(engine->playingSoundCount() == 1);

  second.script = nullptr;
  second.artboard = nullptr;
  CHECK(engine->playingSoundCount() == 0);
}

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
// A script's sound plays on the runtime engine, not its artboard's own.
TEST_CASE(
    "A script's sound stops with an artboard that has its own engine",
    "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);
  rcp<AudioEngine> own = AudioEngine::Make(2, 44100);
  ScriptingTest test("return function(source) return Audio.play(source) end");
#ifdef WITH_RIVE_TOOLS
  test.vm()->context()->isPlaying(true);
#endif
  lua_State* L = test.state();
  auto source = loadSource("assets/audio/what.wav");

  ScriptArtboard owner(test.serializer());
  owner.artboard->audioEngine(own);
  playFrom(L, owner.script.get(), source);
  lua_pop(L, 1);
  REQUIRE(engine->playingSoundCount() == 1);

  owner.script = nullptr;
  owner.artboard = nullptr;
  CHECK(engine->playingSoundCount() == 0);
}
#endif

TEST_CASE("A script can still use a sound whose artboard is deleted", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);
  ScriptingTest test(
      R"(
return function(source) return Audio.play(source) end,
  function(sound)
    sound.volume = 0.25
    sound:stop()
    return sound.volume
  end
)",
      2);
#ifdef WITH_RIVE_TOOLS
  test.vm()->context()->isPlaying(true);
#endif
  lua_State* L = test.state();
  int useRef = lua_ref(L, -1);
  lua_pop(L, 1);
  auto source = loadSource("assets/audio/what.wav");

  ScriptArtboard owner(test.serializer());
  playFrom(L, owner.script.get(), source);
  int soundRef = lua_ref(L, -1);
  lua_pop(L, 1);
  REQUIRE(engine->playingSoundCount() == 1);

  owner.script = nullptr;
  owner.artboard = nullptr;
  CHECK(engine->playingSoundCount() == 0);

  rive_lua_pushRef(L, useRef);
  rive_lua_pushRef(L, soundRef);
  REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
  CHECK(lua_tonumber(L, -1) == Approx(0.25f));
  lua_pop(L, 1);
  lua_unref(L, useRef);
  lua_unref(L, soundRef);
}

#ifdef WITH_RIVE_TOOLS
// luau_listener_sound.riv (source: assets/rml/luau_listener_sound/): Setter
// sets score in its second advance, which runs Listener's listener at once.
// Its sound has no owner, as on the wasm backend.
TEST_CASE("A Luau listener's sound has no owner", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/luau_listener_sound.riv");
  file->scriptingVM()->context()->isPlaying(true);
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
  listener = nullptr;
  CHECK(engine->playingSoundCount() == 1);
  file = nullptr;
  CHECK(engine->playingSoundCount() == 1);
}

// Relay's listener makes an instance of Player and advances it, then plays.
// The instance's hooks own their sounds; Relay's own sound has none.
TEST_CASE("A hook a Luau listener runs owns its sounds", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/luau_listener_sound.riv");
  file->scriptingVM()->context()->isPlaying(true);
  auto setter = file->artboardNamed("Setter");
  auto relay = file->artboardNamed("Relay");
  REQUIRE(setter != nullptr);
  REQUIRE(relay != nullptr);
  auto game = file->createViewModelInstance("Game");
  REQUIRE(game != nullptr);
  setter->bindViewModelInstance(game);
  relay->bindViewModelInstance(game);

  // The deck input's own instance of Player plays from init and advance.
  relay->advance(1.0f / 60.0f);
  setter->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 2);
  relay->advance(1.0f / 60.0f);
  setter->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 5);

  setter = nullptr;
  CHECK(engine->playingSoundCount() == 5);

  // The file's VM holds both instances until the file goes.
  relay = nullptr;
  file = nullptr;
  CHECK(engine->playingSoundCount() == 1);
}

// luau_listener_owner.riv (source: assets/rml/luau_listener_owner/): Host
// drops its instance of Inner, then runs a listener of Inner's script during
// which the collector deletes that instance; the listener then plays. They'd
// read freed memory under ASan if listeners ran as their property's owner.
static void playAfterOwnerCollected(bool nested)
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/luau_listener_owner.riv");
  file->scriptingVM()->context()->isPlaying(true);
  auto host = file->artboardNamed("Host");
  REQUIRE(host != nullptr);
  auto game = file->createViewModelInstance("Game");
  REQUIRE(game != nullptr);
  game->propertyValue("nested")->as<ViewModelInstanceBoolean>()->propertyValue(
      nested);
  host->bindViewModelInstance(game);

  host->advance(1.0f / 60.0f);
  CHECK(engine->playingSoundCount() == 0);
  host->advance(1.0f / 60.0f);
  CHECK(engine->playingSoundCount() == 1);

  // A listener's sound has no owner, so nothing stops it.
  host = nullptr;
  CHECK(engine->playingSoundCount() == 1);
}

TEST_CASE(
    "A listener whose artboard is collected in it plays unowned",
    "[audio]")
{
  playAfterOwnerCollected(false);
}

TEST_CASE(
    "A listener whose artboard is collected in a nested listener plays unowned",
    "[audio]")
{
  playAfterOwnerCollected(true);
}

// luau_kept_sound.riv: another artboard's script can't restart, through a
// module both share, a sound whose artboard is deleted.
TEST_CASE(
    "A Luau script can't restart a sound whose artboard is deleted",
    "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = ReadRiveFile("assets/luau_kept_sound.riv");
  file->scriptingVM()->context()->isPlaying(true);
  kept_sound::checkOverForGood(file.get(), engine.get());
}

// luau_promise_owner.riv (source: assets/rml/luau_promise_owner/): its bus
// module holds promises that every script requiring it shares. The waiting
// script registers its handlers from init, the settling script settles them
// from its first advance.
static rcp<File> readPromiseOwner()
{
  auto file = ReadRiveFile("assets/luau_promise_owner.riv");
  file->scriptingVM()->context()->isPlaying(true);
  return file;
}

// Resolver's hook runs Waiter's andThen, finally, catch, cancel hook and
// awaiting async function; none of their sounds is Resolver's.
TEST_CASE("A promise handler another script settles has no owner", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = readPromiseOwner();
  auto waiter = file->artboardNamed("Waiter");
  auto resolver = file->artboardNamed("Resolver");
  REQUIRE(waiter != nullptr);
  REQUIRE(resolver != nullptr);
  waiter->advance(1.0f / 60.0f);
  CHECK(engine->playingSoundCount() == 0);
  resolver->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 5);

  resolver = nullptr;
  CHECK(engine->playingSoundCount() == 5);
  waiter = nullptr;
  CHECK(engine->playingSoundCount() == 5);
  file = nullptr;
  CHECK(engine->playingSoundCount() == 5);
}

// Hatcher's handler, run by Spawner's hook, makes and advances an instance
// of Player: the instance's hooks own their sounds, the handler's has none.
TEST_CASE("A hook a promise handler runs owns its sounds", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = readPromiseOwner();
  auto hatcher = file->artboardNamed("Hatcher");
  auto spawner = file->artboardNamed("Spawner");
  REQUIRE(hatcher != nullptr);
  REQUIRE(spawner != nullptr);
  // The deck input's own instance of Player plays from init.
  hatcher->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 1);
  spawner->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 4);

  spawner = nullptr;
  CHECK(engine->playingSoundCount() == 4);

  // The file's VM holds both instances until the file goes.
  hatcher = nullptr;
  file = nullptr;
  CHECK(engine->playingSoundCount() == 1);
}

// Fumbler's handler plays and throws inside Thrower's hook, which then plays:
// that sound is still Thrower's.
TEST_CASE("A hook owns its sounds after a promise handler throws", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = readPromiseOwner();
  auto fumbler = file->artboardNamed("Fumbler");
  auto thrower = file->artboardNamed("Thrower");
  REQUIRE(fumbler != nullptr);
  REQUIRE(thrower != nullptr);
  fumbler->advance(1.0f / 60.0f);
  thrower->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 2);

  thrower = nullptr;
  CHECK(engine->playingSoundCount() == 1);
  fumbler = nullptr;
  file = nullptr;
  CHECK(engine->playingSoundCount() == 1);
}

// Keeper settles its own promises in its own hook. The executor runs as
// Keeper's call and owns its sound; the handlers, like listeners, don't.
TEST_CASE("A promise handler a script settles itself has no owner", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = readPromiseOwner();
  auto keeper = file->artboardNamed("Keeper");
  REQUIRE(keeper != nullptr);
  keeper->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 3);

  keeper = nullptr;
  CHECK(engine->playingSoundCount() == 2);
  file = nullptr;
  CHECK(engine->playingSoundCount() == 2);
}

// Starter's async function plays before its await, as Starter's own call, and
// after it, inside Resolver's hook: only the first sound is Starter's.
TEST_CASE("An async function owns its sounds until it awaits", "[audio]")
{
  rcp<AudioEngine> engine = AudioEngine::MakeAndStore(2, 44100);

  auto file = readPromiseOwner();
  auto starter = file->artboardNamed("Starter");
  auto resolver = file->artboardNamed("Resolver");
  REQUIRE(starter != nullptr);
  REQUIRE(resolver != nullptr);
  starter->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 1);
  resolver->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 2);

  starter = nullptr;
  CHECK(engine->playingSoundCount() == 1);
  resolver = nullptr;
  file = nullptr;
  CHECK(engine->playingSoundCount() == 1);
}
#endif
#endif
