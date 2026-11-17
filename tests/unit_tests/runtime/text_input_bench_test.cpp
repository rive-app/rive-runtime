#ifdef WITH_RIVE_TEXT
#include "rive/text/cursor.hpp"
#include "rive/layout_component.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text_input.hpp"
#include "rive/text/text_input.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_factory.hpp"
#include <catch.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

using namespace rive;

namespace
{
using Clock = std::chrono::steady_clock;

// The median and the best run say what the work costs; the worst also counts
// whatever else the machine was doing.
struct Samples
{
  std::vector<double> ms;

  void add(Clock::time_point from)
  {
    ms.push_back(
        std::chrono::duration<double, std::milli>(Clock::now() - from).count());
  }

  void print(const char* name)
  {
    if (ms.empty())
    {
      return;
    }
    std::sort(ms.begin(), ms.end());
    double total = 0.0;
    for (double sample : ms)
    {
      total += sample;
    }
    printf(
        "  %-28s median %8.4f  best %8.4f  mean %8.4f  worst %8.4f ms (%zu)\n",
        name,
        ms[ms.size() / 2],
        ms.front(),
        total / ms.size(),
        ms.back(),
        ms.size());
  }
};

// Source like lines: indented, of mixed length, one in eight empty.
std::string benchText(int lines)
{
  const char* words[] = {
      "const",
      "value",
      "=",
      "layout",
      "(",
      "width",
      "height",
      ")",
      "return",
      "if",
      "{",
      "}",
      "measure",
      "+",
      "cursor",
      "scroll",
      "->",
      "paragraph",
  };
  std::string text;
  uint32_t seed = 12345;
  for (int line = 0; line < lines; line++)
  {
    seed = seed * 1664525u + 1013904223u;
    if ((seed >> 24) % 8 != 0)
    {
      text.append((seed >> 20) % 4 * 2, ' ');
      int count = 2 + (seed >> 12) % 14;
      for (int word = 0; word < count; word++)
      {
        seed = seed * 1664525u + 1013904223u;
        text += words[(seed >> 16) % 18];
        text += ' ';
      }
    }
    text += '\n';
  }
  return text;
}

rcp<Font> benchFont()
{
  FILE* fp = fopen("assets/fonts/Inter_18pt-Regular.ttf", "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  const size_t length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> bytes(length);
  REQUIRE(fread(bytes.data(), 1, length, fp) == length);
  fclose(fp);
  return HBFont::Decode(bytes);
}

uint32_t lineStart(const std::string& text, int line)
{
  uint32_t index = 0;
  for (int seen = 0; index < text.size() && seen < line; index++)
  {
    if (text[index] == '\n')
    {
      seen++;
    }
  }
  return index;
}

constexpr float kWidth = 640.0f;
constexpr float kViewport = 720.0f;
constexpr float kNoLimit = std::numeric_limits<float>::max();

// What a TextInput does to its raw input each frame it is dirty.
void frame(RawTextInput& input, Factory* factory, float width)
{
  input.measure(width, kNoLimit);
  input.maxWidth(width);
  input.update(factory);
}
} // namespace

// Editing and scrolling a text input that holds a large file; hidden from
// normal runs. BENCH_LINES sets the line count.
TEST_CASE("Bench large text input", "[.bench]")
{
  int lines = getenv("BENCH_LINES") ? atoi(getenv("BENCH_LINES")) : 10000;
  int runs = getenv("BENCH_RUNS") ? atoi(getenv("BENCH_RUNS")) : 200;
  std::string text = benchText(lines);
  rcp<Font> font = benchFont();
  REQUIRE(font != nullptr);
  NoOpFactory factory;
  printf("text input, %d lines, %zu bytes\n", lines, text.size());

  RawTextInput input;
  input.font(font);
  input.fontSize(16.0f);
  input.sizing(TextSizing::autoHeight);
  input.maxWidth(kWidth);

  auto loadStart = Clock::now();
  input.text(text);
  frame(input, &factory, kWidth);
  float height = input.bounds().height();
  float middle = std::floor(height / 2.0f);
  input.visibleRange(middle, middle + kViewport, &factory);
  Samples load;
  load.add(loadStart);
  load.print("load");

  uint32_t caret = lineStart(text, lines / 2) + 3;
  input.cursor(Cursor::collapsed(CursorPosition(caret)));
  frame(input, &factory, kWidth);

  Samples type;
  for (int i = 0; i < runs; i++)
  {
    auto start = Clock::now();
    input.insert("x");
    frame(input, &factory, kWidth);
    type.add(start);
  }
  type.print("type a character");

  Samples newline;
  for (int i = 0; i < runs; i++)
  {
    auto start = Clock::now();
    input.insert("\n");
    frame(input, &factory, kWidth);
    newline.add(start);
  }
  newline.print("type a newline");

  Samples backspace;
  for (int i = 0; i < runs; i++)
  {
    auto start = Clock::now();
    input.backspace(-1);
    frame(input, &factory, kWidth);
    backspace.add(start);
  }
  backspace.print("backspace");

  Samples undo;
  for (int i = 0; i < runs; i++)
  {
    auto start = Clock::now();
    input.undo();
    frame(input, &factory, kWidth);
    undo.add(start);
  }
  undo.print("undo");

  Samples caretMove;
  for (int i = 0; i < runs; i++)
  {
    auto start = Clock::now();
    input.cursorRight();
    frame(input, &factory, kWidth);
    caretMove.add(start);
  }
  caretMove.print("move the caret");

  Samples scroll;
  float y = middle;
  for (int i = 0; i < runs * 4; i++)
  {
    auto start = Clock::now();
    y += 24.0f;
    input.visibleRange(y, y + kViewport, &factory);
    scroll.add(start);
  }
  scroll.print("scroll 24px a frame");

  Samples fling;
  y = 0.0f;
  for (int i = 0; i < runs; i++)
  {
    auto start = Clock::now();
    y = std::fmod(y + kViewport * 4.0f, height);
    input.visibleRange(y, y + kViewport, &factory);
    fling.add(start);
  }
  fling.print("scroll 4 viewports a frame");

  Samples resize;
  for (int i = 0; i < runs / 4; i++)
  {
    auto start = Clock::now();
    frame(input, &factory, i % 2 == 0 ? kWidth - 80.0f : kWidth);
    resize.add(start);
  }
  resize.print("resize");
}

// The same edit through a TextInput in an artboard, which adds the bound text
// property and the layout pass.
TEST_CASE("Bench large text input in an artboard", "[.bench]")
{
  int lines = getenv("BENCH_LINES") ? atoi(getenv("BENCH_LINES")) : 10000;
  int runs = getenv("BENCH_RUNS") ? atoi(getenv("BENCH_RUNS")) : 200;
  std::string text = benchText(lines);
  auto file = ReadRiveFile("assets/text_input.riv");
  auto artboard = file->artboardNamed("Text Input - Multiline");
  REQUIRE(artboard != nullptr);
  auto textInput = artboard->objects<TextInput>().first();
  REQUIRE(textInput != nullptr);
  // TextInput -> Text Container -> Scroll Content -> Viewport. The file leaves
  // the viewport unclipped, which builds every line.
  textInput->parent()->parent()->parent()->as<LayoutComponent>()->clip(true);
  printf("artboard text input, %d lines, %zu bytes\n", lines, text.size());

  auto loadStart = Clock::now();
  textInput->text(text);
  artboard->advance(0.0f);
  Samples load;
  load.add(loadStart);
  load.print("load");

  uint32_t caret = lineStart(text, lines / 2) + 3;
  textInput->rawTextInput()->cursor(Cursor::collapsed(CursorPosition(caret)));
  artboard->advance(0.0f);

  Samples type;
  for (int i = 0; i < runs; i++)
  {
    auto start = Clock::now();
    textInput->textInput("x");
    artboard->advance(0.0f);
    type.add(start);
  }
  type.print("type a character");

  Samples undo;
  for (int i = 0; i < runs; i++)
  {
    auto start = Clock::now();
    textInput->keyInput(
        Key::z,
        KeyModifiers::meta | KeyModifiers::ctrl,
        true,
        false);
    artboard->advance(0.0f);
    undo.add(start);
  }
  undo.print("undo");
}
#endif
