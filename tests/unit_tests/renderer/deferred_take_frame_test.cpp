/*
 * Copyright 2026 Rive
 */

// takeFrame moves a session's recorded bytes into the frame: an import puts
// every image's bytes there, so a copy costs megabytes while they decode.

#include "rive/file.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive_file_reader.hpp"

#include <catch.hpp>

#include <vector>

using namespace rive;

namespace
{
constexpr size_t kImageBytes = 4 << 20;

// Stands in for an encoded image; the session only stores the bytes.
std::vector<uint8_t> imageBytes(size_t size, uint8_t seed)
{
  std::vector<uint8_t> bytes(size);
  for (size_t i = 0; i < size; ++i)
  {
    bytes[i] = static_cast<uint8_t>(seed + i);
  }
  return bytes;
}

void recordImage(
    cmd::DeferredSession& session,
    const std::vector<uint8_t>& bytes)
{
  session.decodeImage(Span<const uint8_t>(bytes.data(), bytes.size()));
}
} // namespace

TEST_CASE(
    "a taken frame carries the bytes a snapshot would copy",
    "[deferred][take_frame]")
{
  const auto bytes = imageBytes(1024, 7);

  cmd::DeferredSession copied(rive::ore::ReplayCaps{});
  recordImage(copied, bytes);
  cmd::DeferredFrame snapshot = cmd::snapshotFrame(copied);

  cmd::DeferredSession moved(rive::ore::ReplayCaps{});
  recordImage(moved, bytes);
  cmd::DeferredFrame taken = cmd::takeFrame(moved);

  REQUIRE_FALSE(taken.commands.empty());
  REQUIRE(taken.blobs.size() >= bytes.size());
  CHECK(taken.commands == snapshot.commands);
  CHECK(taken.blobs == snapshot.blobs);
  CHECK(taken.oreCommands == snapshot.oreCommands);
  CHECK(taken.oreBlobs == snapshot.oreBlobs);
}

TEST_CASE(
    "taking a frame leaves nothing of it in the session",
    "[deferred][take_frame]")
{
  cmd::DeferredSession session(rive::ore::ReplayCaps{});
  recordImage(session, imageBytes(kImageBytes, 1));

  cmd::DeferredFrame frame = cmd::takeFrame(session);
  REQUIRE(frame.blobs.size() >= kImageBytes);
  CHECK(session.commandBuffer().blobBytes().size() == 0);
  // A large frame must not leave its capacity behind either.
  CHECK(session.commandBuffer().blobCapacity() < kImageBytes);
}

TEST_CASE(
    "blobs reserved for an import do not grow while it records",
    "[deferred][take_frame]")
{
  const size_t total = 3 * kImageBytes;

  cmd::DeferredSession session(rive::ore::ReplayCaps{});
  session.commandBuffer().reserveBlobs(total);
  const size_t reserved = session.commandBuffer().blobCapacity();
  REQUIRE(reserved >= total);
  for (uint8_t i = 0; i < 3; ++i)
  {
    recordImage(session, imageBytes(kImageBytes, i));
  }
  CHECK(session.commandBuffer().blobCapacity() == reserved);
}

TEST_CASE(
    "importing a file through a session reserves its size",
    "[deferred][take_frame]")
{
  const auto bytes = ReadFile("assets/image_fit_alignment.riv");
  REQUIRE(bytes.size() > 0);

  cmd::DeferredSession session(rive::ore::ReplayCaps{});
  auto file = File::import(bytes, &session);
  REQUIRE(file != nullptr);
  CHECK(session.commandBuffer().blobCapacity() >= bytes.size());
  CHECK(
      session.commandBuffer().blobBytes().size() <=
      session.commandBuffer().blobCapacity());
}
