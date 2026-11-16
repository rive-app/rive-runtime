/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/cmd/replay_time.hpp"

#include <catch.hpp>

using namespace rive::cmd;

TEST_CASE("a cold load's slow first frame sets no replay time", "[replay_time]")
{
  ReplayTime time;
  CHECK(time.note(4500) == 0);
  for (int frame = 2; frame <= 20; ++frame)
  {
    CAPTURE(frame);
    CHECK(time.note(6) <= 6);
  }
}

TEST_CASE("the replay time starts with the third frame", "[replay_time]")
{
  ReplayTime time;
  CHECK(time.note(30) == 0);
  CHECK(time.note(38) == 0);
  CHECK(time.note(31) == 31);
}

TEST_CASE(
    "a frame timed between two polls does not lower the replay time",
    "[replay_time]")
{
  ReplayTime time;
  time.note(30);
  time.note(0);
  CHECK(time.note(30) == 30);
}

TEST_CASE(
    "one slow frame later on does not move the replay time",
    "[replay_time]")
{
  ReplayTime time;
  for (int frame = 1; frame <= 9; ++frame)
  {
    time.note(6);
  }
  CHECK(time.note(900) == 6);
  CHECK(time.note(6) == 6);
}

TEST_CASE(
    "the replay time averages the middle of its last nine frames",
    "[replay_time]")
{
  ReplayTime time;
  float ms = 0;
  for (int frame = 1; frame <= 9; ++frame)
  {
    ms = time.note(frame * frame);
  }
  CHECK(ms == 27);
  for (int frame = 1; frame <= 9; ++frame)
  {
    ms = time.note(200);
  }
  CHECK(ms == 200);
}
