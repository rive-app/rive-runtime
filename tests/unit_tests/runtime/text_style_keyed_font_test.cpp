#include <rive/file.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/assets/font_asset.hpp>
#include <rive/text/text.hpp>
#include <rive/text/text_style.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>

using namespace rive;

// assets/text_style_keyed_font.riv (from rive_core's
// generate_text_style_keyed_font_riv_test.dart): one Text whose style's font
// is keyed in "swap", LibreBodoni ("Bodoni") at frame 0 and the Twemoji
// subset ("Emoji") at frame 30 of 60 fps. Emoji reaches the file only
// through its key.
TEST_CASE("A keyed font swaps the style's font", "[text]")
{
  auto file = ReadRiveFile("assets/text_style_keyed_font.riv");
  auto assets = file->assets();
  REQUIRE(assets.size() == 2);
  rcp<FileAsset> bodoni;
  rcp<FileAsset> emoji;
  for (auto asset : assets)
  {
    REQUIRE(asset->is<FontAsset>());
    (asset->name() == "Bodoni" ? bodoni : emoji) = asset;
  }
  REQUIRE(bodoni != nullptr);
  REQUIRE(emoji != nullptr);
  REQUIRE(emoji->name() == "Emoji");

  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  // Names don't export; the file has one of each.
  TextStyle* style = nullptr;
  Text* text = nullptr;
  for (auto object : artboard->objects())
  {
    if (object != nullptr && object->is<TextStyle>())
    {
      style = object->as<TextStyle>();
    }
    else if (object != nullptr && object->is<Text>())
    {
      text = object->as<Text>();
    }
  }
  REQUIRE(style != nullptr);
  REQUIRE(text != nullptr);

  auto swap = artboard->animationNamed("swap");
  REQUIRE(swap != nullptr);

  swap->advanceAndApply(0.0f);
  artboard->advance(0.0f);
  CHECK(style->asset() == bodoni);
  CHECK(style->font() == bodoni->as<FontAsset>()->font());
  auto bodoniWidth = text->localBounds().width();

  // Past the key at 0.5s: the instance (a clone) resolves the key's index
  // against the file it was imported from, and the text reshapes.
  swap->advanceAndApply(0.75f);
  artboard->advance(0.0f);
  CHECK(style->asset() == emoji);
  CHECK(style->font() == emoji->as<FontAsset>()->font());
  CHECK(text->localBounds().width() != bodoniWidth);

  // And back.
  swap->time(0.0f);
  swap->apply();
  artboard->advance(0.0f);
  CHECK(style->asset() == bodoni);
  CHECK(text->localBounds().width() == bodoniWidth);
}
