#include <rive/file.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/text/font_hb.hpp>
#include <rive/text/text.hpp>
#include <rive/text/text_language.hpp>
#include <rive/text/text_value_run.hpp>
#include <rive/text/utf.hpp>
#include <rive/text_engine.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstring>
#include <string>
#include <vector>

using namespace rive;

namespace
{
// Every request a test's fallback proc sees: the codepoint and the languages
// it was asked for.
struct FallbackLog
{
  std::vector<std::pair<Unichar, std::vector<std::string>>> requests;
  // FallbackRequest::hasTextLanguage, per request.
  std::vector<bool> ownLanguage;
  // Handed back when the text's most preferred language is "ja" or
  // "zh-Hans", the way a host picks a Japanese or a Chinese font.
  rcp<Font> japanese;
  rcp<Font> chinese;
};
FallbackLog* g_log = nullptr;

rcp<Font> recordRequest(const Font::FallbackRequest& request)
{
  std::vector<std::string> languages(
      request.languages.begin(),
      request.languages.end());
  g_log->requests.push_back({request.missing, languages});
  g_log->ownLanguage.push_back(request.hasTextLanguage);
  if (request.fallbackIndex > 0 || languages.empty())
  {
    return nullptr;
  }
  if (languages.front() == "ja")
  {
    return g_log->japanese;
  }
  if (languages.front() == "zh-Hans")
  {
    return g_log->chinese;
  }
  return nullptr;
}

// Installs [log]'s proc for the test's scope and leaves the process as the
// other tests expect it: no procs, no system languages, an empty table.
struct ScopedFallbackLog
{
  FallbackLog log;
  ScopedFallbackLog()
  {
    TextLanguages::resetForTesting();
    g_log = &log;
    Font::gFallbackRequestProc = recordRequest;
  }
  ~ScopedFallbackLog()
  {
    Font::gFallbackRequestProc = nullptr;
    g_log = nullptr;
    TextLanguages::resetForTesting();
  }
};

rcp<Font> loadFont(const char* path)
{
  auto bytes = ReadFile(path);
  return HBFont::Decode(bytes);
}

std::vector<Text*> texts(ArtboardInstance* artboard)
{
  std::vector<Text*> found;
  for (auto object : artboard->objects())
  {
    if (object != nullptr && object->is<Text>())
    {
      found.push_back(object->as<Text>());
    }
  }
  return found;
}

std::vector<Unichar> codepoints(const char* utf8)
{
  std::vector<Unichar> out;
  const uint8_t* ptr = (const uint8_t*)utf8;
  const uint8_t* end = ptr + strlen(utf8);
  while (ptr < end)
  {
    out.push_back(UTF::NextUTF8(&ptr, end));
  }
  return out;
}

// What a host changes that text reshapes for without a change of its own,
// one per section: the system languages inherited text shapes with, or the
// fallback fonts (such as one fetched for a language once text uses it).
void changeHostText()
{
  SECTION("new system languages")
  {
    std::vector<std::string> system = {"tr"};
    TextLanguages::setSystemLanguages(system);
  }
  SECTION("new fallback fonts") { Font::fallbackFontsChanged(); }
}
} // namespace

// A language is any string, usually a BCP 47 tag: it reaches the shaper and
// the host's fallback requests exactly as authored, so a host can use a
// format of its own.
TEST_CASE("languages intern exactly as written", "[text_language]")
{
  TextLanguages::resetForTesting();
  auto ja = TextLanguages::intern("ja");
  CHECK(ja != TextLanguages::none);
  CHECK(TextLanguages::intern("ja") == ja);
  CHECK(TextLanguages::tag(ja) == "ja");
  // Other spellings are other strings.
  auto upper = TextLanguages::intern("JA");
  CHECK(upper != ja);
  CHECK(TextLanguages::tag(upper) == "JA");
  for (const char* custom : {"zh_hans", "ja, en", "japanese", "x-a", " ja "})
  {
    auto id = TextLanguages::intern(custom);
    CHECK(id != TextLanguages::none);
    CHECK(TextLanguages::tag(id) == custom);
  }
  CHECK(TextLanguages::intern("") == TextLanguages::none);
  CHECK(TextLanguages::tag(TextLanguages::none).empty());

  // The table holds 255; past that a new one inherits.
  for (size_t i = 0; i < TextLanguages::maxCount; i++)
  {
    TextLanguages::intern("custom-" + std::to_string(i));
  }
  CHECK(TextLanguages::intern("one more") == TextLanguages::none);
  // Ones already in keep their slot.
  CHECK(TextLanguages::intern("ja") == ja);
  TextLanguages::resetForTesting();
}

TEST_CASE("system languages resolve inherited text", "[text_language]")
{
  TextLanguages::resetForTesting();
  auto generation = TextLanguages::generation();
  // Kept as written; only empty ones and repeats go.
  std::vector<std::string> system = {"en-US", "", "ja", "en-US", "ja_JP"};
  TextLanguages::setSystemLanguages(system);
  CHECK(TextLanguages::generation() != generation);
  CHECK(
      TextLanguages::systemLanguages() ==
      std::vector<std::string>{"en-US", "ja", "ja_JP"});

  // The same list again is no change.
  generation = TextLanguages::generation();
  TextLanguages::setSystemLanguages(system);
  CHECK(TextLanguages::generation() == generation);

  auto ja = TextLanguages::intern("ja");
  auto ko = TextLanguages::intern("ko");
  CHECK(TextLanguages::shapingLanguage(TextLanguages::none) == "en-US");
  CHECK(TextLanguages::shapingLanguage(ko) == "ko");
  CHECK(
      TextLanguages::fallbackLanguages(TextLanguages::none) ==
      std::vector<std::string>{"en-US", "ja", "ja_JP"});
  CHECK(
      TextLanguages::fallbackLanguages(ko) ==
      std::vector<std::string>{"ko", "en-US", "ja", "ja_JP"});
  // The text's own language isn't repeated.
  CHECK(
      TextLanguages::fallbackLanguages(ja) ==
      std::vector<std::string>{"ja", "en-US", "ja_JP"});
  // A custom format reaches the request as authored.
  auto custom = TextLanguages::intern("brand:jp");
  CHECK(TextLanguages::shapingLanguage(custom) == "brand:jp");
  CHECK(TextLanguages::fallbackLanguages(custom).front() == "brand:jp");
  TextLanguages::resetForTesting();
  CHECK(TextLanguages::shapingLanguage(TextLanguages::none).empty());
}

// assets/text_language.riv (from rive_core's
// generate_text_language_riv_test.dart): a Text inheriting the system
// language whose five runs are each ja, a zh-Hans Text with one run that
// inherits it, a Text with no language, and a Text ("直") whose language is
// bound to the view model's "lang" string (ja). Each authored tag ships
// once, as a manifest name the objects index with languageId.
TEST_CASE(
    "languages resolve from the manifest while decoding",
    "[text_language]")
{
  auto file = ReadRiveFile("assets/text_language.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  auto found = texts(artboard.get());
  REQUIRE(found.size() == 4);

  CHECK(found[0]->language().empty());
  REQUIRE(found[0]->runs().size() == 5);
  for (auto run : found[0]->runs())
  {
    CHECK(run->language() == "ja");
  }
  CHECK(found[1]->language() == "zh-Hans");
  REQUIRE(found[1]->runs().size() == 1);
  CHECK(found[1]->runs()[0]->language().empty());
  CHECK(found[2]->language().empty());
  CHECK(found[2]->runs()[0]->language().empty());
}

// assets/text_language_no_assets.riv has the same texts but no font, so the
// manifest is its only asset: nothing resolves the manifest's importer
// before the artboard reads, which used to leave every name undecoded.
TEST_CASE(
    "a manifest that is the only asset decodes before the artboard",
    "[text_language]")
{
  auto file = ReadRiveFile("assets/text_language_no_assets.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  auto found = texts(artboard.get());
  REQUIRE(found.size() == 4);
  CHECK(found[0]->runs()[0]->language() == "ja");
  CHECK(found[1]->language() == "zh-Hans");
}

TEST_CASE(
    "fallback requests list the text's language, then the system's",
    "[text_language]")
{
  ScopedFallbackLog scope;
  std::vector<std::string> system = {"en-US"};
  TextLanguages::setSystemLanguages(system);
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  // Inter has no Arabic, so every letter asks for a fallback.
  auto text = codepoints("ب");

  auto shape = [&](TextLanguageId language) {
    scope.log.requests.clear();
    scope.log.ownLanguage.clear();
    TextRun run = {font, 32.0f, -1.0f, 0.0f, (uint32_t)text.size(), 0, 0};
    run.language = language;
    return font->shapeText(text, Span<TextRun>(&run, 1));
  };

  shape(TextLanguages::intern("ja"));
  REQUIRE(!scope.log.requests.empty());
  CHECK(scope.log.requests[0].first == text[0]);
  CHECK(
      scope.log.requests[0].second == std::vector<std::string>{"ja", "en-US"});
  CHECK(scope.log.ownLanguage[0]);

  shape(TextLanguages::none);
  REQUIRE(!scope.log.requests.empty());
  CHECK(scope.log.requests[0].second == std::vector<std::string>{"en-US"});
  CHECK(!scope.log.ownLanguage[0]);
}

TEST_CASE("one codepoint falls back by its text's language", "[text_language]")
{
  ScopedFallbackLog scope;
  // Two fonts with the same letters stand in for a Japanese and a Chinese
  // font: the request's language picks between them.
  scope.log.japanese = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  scope.log.chinese = loadFont(
      "assets/NotoSansArabic-VariableFont_wdth,wght.ttf");
  REQUIRE(scope.log.japanese != nullptr);
  REQUIRE(scope.log.chinese != nullptr);
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  auto text = codepoints("ب");

  auto fallbackFont = [&](const char* language) {
    TextRun run = {font, 32.0f, -1.0f, 0.0f, (uint32_t)text.size(), 0, 0};
    run.language = TextLanguages::intern(language);
    auto paragraphs = font->shapeText(text, Span<TextRun>(&run, 1));
    REQUIRE(paragraphs.size() == 1);
    REQUIRE(paragraphs[0].runs.size() == 1);
    CHECK(paragraphs[0].runs[0].language == run.language);
    return paragraphs[0].runs[0].font;
  };

  CHECK(fallbackFont("ja") == scope.log.japanese);
  CHECK(fallbackFont("zh-Hans") == scope.log.chinese);
}

TEST_CASE("a Text's language reaches the fallback request", "[text_language]")
{
  ScopedFallbackLog scope;
  std::vector<std::string> system = {"en-US"};
  TextLanguages::setSystemLanguages(system);
  auto file = ReadRiveFile("assets/text_language.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  artboard->advance(0.0f);

  // The file's font (LibreBodoni) has no Han, so each text asked: the runs
  // set to ja, and the run inheriting its zh-Hans Text.
  bool sawJapanese = false;
  bool sawChinese = false;
  for (const auto& request : scope.log.requests)
  {
    if (request.second == std::vector<std::string>{"ja", "en-US"})
    {
      sawJapanese = true;
    }
    else if (request.second == std::vector<std::string>{"zh-Hans", "en-US"})
    {
      sawChinese = true;
    }
  }
  CHECK(sawJapanese);
  CHECK(sawChinese);
}

TEST_CASE(
    "new system languages or fallback fonts reshape on the next advance",
    "[text_language]")
{
  ScopedFallbackLog scope;
  auto file = ReadRiveFile("assets/text_language.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  artboard->advance(0.0f);
  auto plain = texts(artboard.get())[2];
  auto shapes = plain->shapeCount();
  artboard->advance(0.0f);
  CHECK(plain->shapeCount() == shapes);

  changeHostText();
  artboard->advance(0.0f);
  CHECK(plain->shapeCount() > shapes);
}

// The editor turns fallbacks off and on around its own shaping. No other
// fonts are picked from, so text already shaped stays as it is.
TEST_CASE("turning fallbacks off and on doesn't reshape", "[text_language]")
{
  ScopedFallbackLog scope;
  auto file = ReadRiveFile("assets/text_language.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  artboard->advance(0.0f);
  auto plain = texts(artboard.get())[2];
  auto shapes = plain->shapeCount();

  Font::gFallbackProcEnabled = false;
  artboard->advance(0.0f);
  Font::gFallbackProcEnabled = true;
  artboard->advance(0.0f);
  CHECK(plain->shapeCount() == shapes);
}

TEST_CASE("a view model string drives a Text's language", "[text_language]")
{
  ScopedFallbackLog scope;
  std::vector<std::string> system = {"en-US"};
  TextLanguages::setSystemLanguages(system);
  auto file = ReadRiveFile("assets/text_language.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  auto bound = texts(artboard.get())[3];
  auto vmi = file->createViewModelInstance(artboard->viewModelId(), 0);
  REQUIRE(vmi != nullptr);
  artboard->bindViewModelInstance(vmi);
  artboard->advance(0.0f);
  CHECK(bound->language() == "ja");

  auto requested = [&](const std::vector<std::string>& languages) {
    for (const auto& request : scope.log.requests)
    {
      if (request.first == 0x76F4 && request.second == languages)
      {
        return true;
      }
    }
    return false;
  };
  CHECK(requested({"ja", "en-US"}));

  // Flip it: the text reshapes and asks with the new language.
  auto lang = vmi->propertyValue("lang");
  REQUIRE(lang != nullptr);
  REQUIRE(lang->is<ViewModelInstanceString>());
  scope.log.requests.clear();
  auto shapes = bound->shapeCount();
  lang->as<ViewModelInstanceString>()->propertyValue("zh-Hans");
  artboard->advance(0.0f);
  CHECK(bound->language() == "zh-Hans");
  CHECK(bound->shapeCount() > shapes);
  CHECK(requested({"zh-Hans", "en-US"}));
}

TEST_CASE("a bound language reshapes the text", "[text_language]")
{
  ScopedFallbackLog scope;
  auto file = ReadRiveFile("assets/text_language.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  artboard->advance(0.0f);
  auto plain = texts(artboard.get())[2];
  auto shapes = plain->shapeCount();

  // Data binding sets language like this.
  plain->language("ko");
  CHECK(plain->languageSlot() == TextLanguages::intern("ko"));
  artboard->advance(0.0f);
  CHECK(plain->shapeCount() > shapes);
}

// A list row whose work would do nothing sleeps (see quiet_rows_test.cpp),
// and a sleeping row's text never advanced to notice new system languages
// or fallback fonts.
TEST_CASE(
    "quiet list rows reshape their text for new system languages or "
    "fallback fonts",
    "[text_language]")
{
  TextLanguages::resetForTesting();
  auto file = ReadRiveFile("assets/artboard_list_overrides.riv");
  auto artboard = file->artboardNamed("Main");
  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  auto viewModelId = artboard->viewModelId();
  auto viewModelInstance = viewModelId == -1
                               ? file->createViewModelInstance(artboard.get())
                               : file->createViewModelInstance(viewModelId, 0);
  stateMachine->bindViewModelInstance(viewModelInstance);
  stateMachine->advanceAndApply(0.1f);

  auto lists = artboard->find<ArtboardComponentList>();
  REQUIRE(lists.size() == 1);
  auto list = lists[0];
  Text* rowText = nullptr;
  size_t textRow = 0;
  for (size_t row = 0; row < list->artboardCount() && rowText == nullptr; row++)
  {
    for (auto object : list->artboardInstance((int)row)->objects())
    {
      if (object != nullptr && object->is<Text>())
      {
        rowText = object->as<Text>();
        textRow = row;
        break;
      }
    }
  }
  REQUIRE(rowText != nullptr);

  // Long enough for the rows to go quiet.
  for (int i = 0; i < 90; i++)
  {
    stateMachine->advanceAndApply(1.0f / 60.0f);
  }
  REQUIRE(
      list->artboardInstance((int)textRow)->quietHostRow() ==
      (uint32_t)textRow);
  auto shapes = rowText->shapeCount();

  changeHostText();
  stateMachine->advanceAndApply(1.0f / 60.0f);
  stateMachine->advanceAndApply(1.0f / 60.0f);
  CHECK(rowText->shapeCount() > shapes);
  TextLanguages::resetForTesting();
}

// A sleeping row whose only work is a list of its own: that list's rows
// sleep too, and the row has to stay awake until the list has advanced to
// wake them.
TEST_CASE(
    "quiet rows reshape the text of a list they host for new system "
    "languages or fallback fonts",
    "[text_language]")
{
  TextLanguages::resetForTesting();
  auto file = ReadRiveFile("assets/quiet_nested_list_text.riv");
  auto artboard = file->artboardNamed("Main");
  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  stateMachine->bindViewModelInstance(
      file->createViewModelInstance(artboard->viewModelId(), 0));
  auto rows = artboard->find<ArtboardComponentList>("Rows");
  REQUIRE(rows != nullptr);

  // Long enough for the rows of both lists to go quiet.
  for (int i = 0; i < 90; i++)
  {
    stateMachine->advanceAndApply(1.0f / 60.0f);
  }
  REQUIRE(rows->artboardCount() == 2);
  auto row = rows->artboardInstance(0);
  REQUIRE(row->quietHostRow() == 0);
  auto words = row->find<ArtboardComponentList>("Words");
  REQUIRE(words != nullptr);
  REQUIRE(words->artboardCount() == 2);
  REQUIRE(words->artboardInstance(0)->quietHostRow() == 0);
  auto word = words->artboardInstance(0)->find<Text>("Label");
  REQUIRE(word != nullptr);
  auto shapes = word->shapeCount();
  REQUIRE(shapes > 0);

  changeHostText();
  stateMachine->advanceAndApply(1.0f / 60.0f);
  stateMachine->advanceAndApply(1.0f / 60.0f);
  CHECK(word->shapeCount() > shapes);
  TextLanguages::resetForTesting();
}

// Runs in the same font and size but different languages: the ellipsis cut
// from the second takes its language (and its forms and fallback), rather
// than keeping the first run's.
TEST_CASE(
    "an ellipsis takes the language of the run it cuts",
    "[text_language]")
{
  TextLanguages::resetForTesting();
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  auto first = codepoints("Hi ");
  auto second = codepoints("there, this line is far too long to fit");
  std::vector<Unichar> text = first;
  text.insert(text.end(), second.begin(), second.end());
  auto ja = TextLanguages::intern("ja");
  auto zh = TextLanguages::intern("zh-Hans");
  TextRun runs[] = {
      {font, 20.0f, -1.0f, 0.0f, (uint32_t)first.size(), 0, 0, 0, ja},
      {font, 20.0f, -1.0f, 0.0f, (uint32_t)second.size(), 0, 1, 0, zh},
  };
  auto shape = font->shapeText(text, Span<TextRun>(runs, 2));
  REQUIRE(shape.size() == 1);
  auto lines = Text::BreakLines(
      shape,
      120.0f,
      TextAlign::left,
      TextWrap::noWrap);
  REQUIRE(lines[0].size() == 1);
  GlyphRun ellipsisRun;
  OrderedLine line(
      shape[0],
      lines[0][0],
      120.0f,
      TextAlign::left,
      true,
      true,
      &ellipsisRun,
      0.0f);
  REQUIRE(ellipsisRun.font != nullptr);
  CHECK(ellipsisRun.language == zh);
  TextLanguages::resetForTesting();
}
