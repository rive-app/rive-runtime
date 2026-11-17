/*
 * Copyright 2026 Rive
 */

// The Luau and AnimaScript kept sound fixtures (assets/rml/*_kept_sound/).

#ifndef _RIVE_KEPT_SOUND_TEST_HELPERS_HPP_
#define _RIVE_KEPT_SOUND_TEST_HELPERS_HPP_

#if defined(WITH_RIVE_AUDIO) && defined(WITH_RIVE_TOOLS)
#include "catch.hpp"
#include "rive/artboard.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/file.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"

#include <algorithm>
#include <vector>

namespace kept_sound
{
enum Op
{
  play = 1,
  resume = 2,
  playAnother = 3,
  everythingElse = 4,
};

inline rive::ViewModelInstanceNumber* number(
    rive::ViewModelInstance* game,
    const char* name)
{
  return game->propertyValue(name)->as<rive::ViewModelInstanceNumber>();
}

inline bool boolean(rive::ViewModelInstance* game, const char* name)
{
  return game->propertyValue(name)
      ->as<rive::ViewModelInstanceBoolean>()
      ->propertyValue();
}

inline void poke(rive::Artboard* poker, rive::ViewModelInstance* game, Op op)
{
  number(game, "op")->propertyValue((float)op);
  poker->advance(1.0f / 60.0f);
  REQUIRE(number(game, "op")->propertyValue() == 0.0f);
}

// The external engine mixes only here, so this hears sounds outside its list.
inline bool heard(rive::AudioEngine* engine)
{
  std::vector<float> out(4096 * 2);
  REQUIRE(engine->readAudioFrames(out.data(), 4096));
  return std::any_of(out.begin(), out.end(), [](float v) { return v != 0; });
}

// Poker uses Keeper's sound after Keeper goes, before and after a new play.
inline void checkOverForGood(rive::File* file, rive::AudioEngine* engine)
{
  auto keeper = file->artboardNamed("Keeper");
  auto poker = file->artboardNamed("Poker");
  REQUIRE(keeper != nullptr);
  REQUIRE(poker != nullptr);
  auto game = file->createViewModelInstance("Game");
  REQUIRE(game != nullptr);
  keeper->bindViewModelInstance(game);
  poker->bindViewModelInstance(game);
  keeper->advance(1.0f / 60.0f);
  REQUIRE(engine->playingSoundCount() == 1);

  keeper = nullptr;
  REQUIRE(engine->playingSoundCount() == 0);

  for (size_t others : {0, 1})
  {
    for (Op op : {play, resume})
    {
      poke(poker.get(), game.get(), op);
      CHECK(engine->playingSoundCount() == others);
      CHECK_FALSE(heard(engine));
      CHECK(boolean(game.get(), "completed"));
    }
    poke(poker.get(), game.get(), everythingElse);
    CHECK(boolean(game.get(), "completed"));
    CHECK_FALSE(boolean(game.get(), "seeked"));
    CHECK(number(game.get(), "time")->propertyValue() == 0.0f);
    CHECK(number(game.get(), "volume")->propertyValue() == 0.5f);
    if (others == 0)
    {
      poke(poker.get(), game.get(), playAnother);
      REQUIRE(engine->playingSoundCount() == 1);
    }
  }
}
} // namespace kept_sound
#endif
#endif
