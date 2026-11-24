#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/scripted_text.hpp"
#include "rive/text/font_hb.hpp"
#include "utils/no_op_renderer.hpp"
#include <set>

using namespace rive;

namespace
{
std::vector<uint8_t> readFile(const char* filename)
{
  FILE* fp = fopen(filename, "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  std::vector<uint8_t> bytes(ftell(fp));
  fseek(fp, 0, SEEK_SET);
  REQUIRE(fread(bytes.data(), 1, bytes.size(), fp) == bytes.size());
  fclose(fp);
  return bytes;
}

// Sets a global while keeping the sandbox's read only state as it was.
template <typename Push>
void setGlobal(lua_State* L, const char* name, Push push)
{
  int readonly = lua_getreadonly(L, LUA_GLOBALSINDEX);
  lua_setreadonly(L, LUA_GLOBALSINDEX, false);
  push();
  lua_setglobal(L, name);
  lua_setreadonly(L, LUA_GLOBALSINDEX, readonly);
}

void setGlobalFont(lua_State* L, const char* name, const char* filename)
{
  setGlobal(L, name, [&] {
    lua_pushfont(L, HBFont::Decode(readFile(filename)));
  });
}

const char* kLatin = "assets/fonts/Inter_18pt-Regular.ttf";
const char* kArabic = "assets/IBMPlexSansArabic-Regular.ttf";

// Runs source with `latin` and `arabic` fonts in scope.
struct TextTest : ScriptingTest
{
  TextTest(const char* source, int numResults = 0, bool errorOk = false)
      : ScriptingTest(source, numResults, errorOk, {}, false)
  {
    setGlobalFont(state(), "latin", kLatin);
    setGlobalFont(state(), "arabic", kArabic);
    execute();
  }
};

void drawWith(ScriptingTest& vm, const char* function)
{
  lua_State* L = vm.state();
  lua_getglobal(L, function);
  auto renderer = vm.serializer()->makeRenderer();
  auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, renderer.get());
  REQUIRE(lua_pcall(L, 1, 0, 0) == LUA_OK);
  CHECK(scriptedRenderer->end());
}
} // namespace

TEST_CASE("text lays out and draws", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        local paint = Paint.with({ color = 0xFF202020 })
        text:append('Hello ', { font = latin, size = 24, paint = paint })
        text:append('world', { font = latin, size = 24, paint = paint })
        assert(text.length == 11)
        assert(not text.isEmpty)
        assert(text.lineCount == 1)
        local min, max = text:bounds()
        assert(min.x == 0 and max.x > 100, 'width ' .. max.x)
        assert(text.sizing == 'autoWidth' and text.align == 'left')
        text.sizing = 'autoHeight'
        text.maxWidth = 60
        assert(text.lineCount > 1, 'lines ' .. text.lineCount)
        function render(renderer: Renderer)
            text:draw(renderer)
        end
    )");
  drawWith(vm, "render");
  CHECK(vm.serializer()->matches("scripted_text_wrapped"));
}

TEST_CASE("text draws every line of a range like draw", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        local paint = Paint.with({ color = 0xFF202020 })
        text:append('Hello ', { font = latin, size = 24, paint = paint })
        text:append('world', { font = latin, size = 24, paint = paint })
        text.sizing = 'autoHeight'
        text.maxWidth = 60
        assert(text.lineCount > 1, 'lines ' .. text.lineCount)
        function render(renderer: Renderer)
            text:drawLines(renderer, 1, text.lineCount)
            -- Empty and out of range ranges draw nothing.
            text:drawLines(renderer, 0, 0)
            text:drawLines(renderer, 2, 1)
            text:drawLines(renderer, text.lineCount + 1, text.lineCount + 3)
        end
    )");
  drawWith(vm, "render");
  CHECK(vm.serializer()->matches("scripted_text_wrapped"));
}

namespace
{
class PathRecorder : public NoOpRenderer
{
public:
  std::set<RenderPath*> paths;
  void drawPath(RenderPath* path, RenderPaint*) override { paths.insert(path); }
};
} // namespace

TEST_CASE("text counts the lines it draws from 1", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        text:append('one two three four five six seven eight nine ten',
            { font = latin, size = 20, paint = Paint.new() })
        text.sizing = 'autoHeight'
        text.maxWidth = 80
        assert(text.lineCount >= 4, 'lines ' .. text.lineCount)
        function textObject(): Text
            return text
        end
        function render(renderer: Renderer, first: number, last: number)
            text:drawLines(renderer, first, last)
        end
    )");
  lua_State* L = vm.state();
  lua_getglobal(L, "textObject");
  REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
  RawText& text = lua_torive<ScriptedText>(L, -1)->layout->text;
  lua_pop(L, 1);

  PathRecorder fromScript;
  lua_getglobal(L, "render");
  auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, &fromScript);
  lua_pushinteger(L, 2);
  lua_pushinteger(L, 3);
  REQUIRE(lua_pcall(L, 3, 0, 0) == LUA_OK);
  CHECK(scriptedRenderer->end());
  REQUIRE(!fromScript.paths.empty());

  // Lines 2 and 3 are the runtime's 1 and 2: drawing those finds the range
  // the script drew, and draws the same paths.
  PathRecorder direct;
  text.renderLines(&direct, 1, 2);
  CHECK(direct.paths == fromScript.paths);
  PathRecorder shifted;
  text.renderLines(&shifted, 2, 3);
  CHECK(shifted.paths != fromScript.paths);
}

TEST_CASE("text justifies its lines", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        text:append('one two three four five six seven eight nine ten eleven',
            { font = latin, size = 20, paint = Paint.new() })
        text.sizing = 'autoHeight'
        text.maxWidth = 150
        text.align = 'justify'
        assert(text.align == 'justify', text.align)
        assert(text.lineCount >= 3, 'lines ' .. text.lineCount)
        for i = 1, text.lineCount - 1 do
            local line = text:line(i)
            assert(line.x == 0, 'x ' .. line.x)
            assert(math.abs(line.width - 150) < 0.01, 'width ' .. line.width)
            local glyphs = line:glyphs()
            local last = glyphs[#glyphs]
            local right = last.x + last.advance
            assert(math.abs(right - 150) < 0.01, 'right ' .. right)
            local x = line.x
            for _, glyph in line do
                assert(math.abs(glyph.x - x) < 0.01, 'glyph at ' .. glyph.x)
                x += glyph.advance
            end
        end
        assert(text:line(text.lineCount).width < 149)
    )");
}

TEST_CASE(
    "text runs carry highlights, underlines and strikethroughs",
    "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        local ink = Paint.with({ color = 0xFF202020 })
        text:append('plain ', { font = latin, size = 20, paint = ink })
        text:append('marked', {
            font = latin,
            size = 20,
            paint = ink,
            highlight = Paint.with({ color = 0xFFFFE066 }),
            underline = ink,
            strikethrough = Paint.with({ color = 0xFFCC0000 }),
        })
        local shadow = Paint.with({ color = 0x40000000 })
        function render(renderer: Renderer)
            text:draw(renderer)
        end
        function renderShadow(renderer: Renderer)
            text:draw(renderer, shadow)
        end
    )");
  lua_State* L = vm.state();
  for (const char* function : {"render", "renderShadow"})
  {
    INFO(function);
    PathRecorder recorder;
    lua_getglobal(L, function);
    auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, &recorder);
    REQUIRE(lua_pcall(L, 1, 0, 0) == LUA_OK);
    CHECK(scriptedRenderer->end());
    // Highlight, underline, both runs' glyphs (their marks make them two
    // styles) and the strikethrough.
    CHECK(recorder.paths.size() == 5);
  }
}

TEST_CASE("text orders bidi runs visually", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        text:append('abc مرحبا def', { font = arabic, size = 20, paint = Paint.new() })
        assert(text.lineCount == 1)
        local line = text:line(1)
        assert(line.direction == 'ltr')
        local glyphs = line:glyphs()
        assert(#glyphs == line.glyphCount)
        local iterated = 0
        for _, glyph in line do
            iterated += 1
        end
        assert(iterated == #glyphs)
        local lastX = -math.huge
        local lastRtlIndex = math.huge
        local runs = {}
        for _, glyph in glyphs do
            assert(glyph.x >= lastX, 'glyphs must advance left to right')
            lastX = glyph.x
            if glyph.direction == 'rtl' then
                assert(glyph.textIndex < lastRtlIndex, 'rtl glyphs read right to left')
                lastRtlIndex = glyph.textIndex
            end
            if runs[#runs] ~= glyph.direction then
                runs[#runs + 1] = glyph.direction
            end
        end
        assert(#runs == 3 and runs[1] == 'ltr' and runs[2] == 'rtl' and runs[3] == 'ltr',
            table.concat(runs, ','))
        assert(glyphs[1].textIndex == 1)
        function render(renderer: Renderer)
            text:draw(renderer)
        end
    )");
  drawWith(vm, "render");
  CHECK(vm.serializer()->matches("scripted_text_bidi"));
}

TEST_CASE(
    "text resolves start and end against the paragraph direction",
    "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        text:append('مرحبا', { font = arabic, size = 20, paint = Paint.new() })
        text.sizing = 'fixed'
        text.maxWidth = 300
        text.maxHeight = 100
        text.align = 'start'
        assert(text.align == 'start')
        assert(text:line(1).direction == 'rtl')
        assert(text:line(1).x > 100, 'rtl start sits on the right')
        text.align = 'end'
        assert(text:line(1).x == 0, 'rtl end sits on the left')
        text.direction = 'ltr'
        assert(text:line(1).x > 100, 'forced ltr moves end to the right')
        text.align = 'left'
        assert(text.align == 'left' and text:line(1).x == 0)
    )");
}

TEST_CASE("text carets and hit tests agree", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        text:append('Hello', { font = latin, size = 20, paint = Paint.new() })
        local x1, top, bottom = text:caret(1)
        assert(x1 == 0 and top == 0 and bottom > top)
        local x4 = text:caret(4)
        assert(x4 > x1)
        assert(text:hitTest(Vector.xy(x4 + 1, (top + bottom) / 2)) == 4)
        assert(text:hitTest(Vector.xy(-10, 0)) == 1)
        local rects = text:selectionRects(1, 3)
        assert(#rects == 2 and rects[1].min.x == 0 and rects[2].max.x <= x4)
        assert(text:line(1).firstIndex == 1 and text:line(1).lastIndex == 5)
    )");
}

TEST_CASE("text lines go stale when the text changes", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        text:append('Hello', { font = latin, size = 20 })
        local line = text:line(1)
        local glyph = line:glyph(1)
        text.maxWidth = 0
        assert(line.width > 0, 'setting the same value keeps the lines')
        text:append(' world', { font = latin, size = 20 })
        local ok, message = pcall(function() return line.width end)
        assert(not ok and message:find('fetch it again'), message)
        ok = pcall(function() return glyph.advance end)
        assert(not ok)
        assert(text:line(1).width > 0)
    )");
}

TEST_CASE("cleared text reads as empty", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        text:append('Hello world', { font = latin, size = 20 })
        assert(text.lineCount == 1)
        text:clear()
        assert(text.isEmpty and text.length == 0)
        assert(text.lineCount == 0 and #text:lines() == 0)
        assert(text:hitTest(Vector.xy(10, 5)) == 1)
        assert(text:caret(1) == nil)
        assert(#text:selectionRects(1, 3) == 0)
        local min, max = text:bounds()
        assert(min.x == 0 and max.x == 0 and max.y == 0)
        text:append('again', { font = latin, size = 20 })
        assert(text.lineCount == 1)
    )");
}

TEST_CASE("text decodes a sequence cut short inside its string", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        text:append('a\xE2b', { font = latin, size = 20 })
        text:append(string.rep('c', 40) .. '\xF0', { font = latin, size = 20 })
        assert(text.length == 44, 'length ' .. text.length)
        assert(text.lineCount == 1)
    )");
}

TEST_CASE("font exposes metrics, options and outlines", "[scripting]")
{
  TextTest vm(R"(
        assert(latin.ascent < 0 and latin.descent > 0)
        assert(latin.capHeight < 0 and latin.xHeight < 0)
        assert(latin.weight == 400 and not latin.isItalic)
        assert(math.abs(latin:lineHeight(10) - (latin.descent - latin.ascent) * 10) < 1e-5)
        assert(latin:hasGlyph(65) and not latin:hasGlyph(0x0627))
        assert(arabic:hasGlyph(0x0627))
        assert(type(latin.axes) == 'table' and type(latin.features) == 'table')
        assert(#latin.features > 0 and #latin.features[1] == 4)
        local variant = latin:withOptions({ wght = 700 }, { liga = 0 })
        assert(variant.ascent == latin.ascent)
        local text = Text.new()
        text:append('A', { font = latin, size = 10 })
        local glyph = text:line(1):glyph(1)
        assert(#latin:glyphPath(glyph.id) > 0)
        assert(#glyph:path() > 0)
        assert(glyph.transform[1] == 10)
        assert(glyph.font.ascent == latin.ascent)
        assert(not glyph.isColor)
        assert(Font.decode(buffer.create(16)) == nil)
    )");
}

TEST_CASE("font tag tables reject keys that are not strings", "[scripting]")
{
  TextTest vm(R"(
        local ok, message = pcall(function()
            return latin:withOptions({ [1] = 700 })
        end)
        assert(not ok and message:find('four letter strings'), message)
        ok, message = pcall(function()
            return latin:withOptions({ weight = 700 })
        end)
        assert(not ok and message:find('four letter tag'), message)
        assert(latin:withOptions({ wght = 700 }) ~= nil)
    )");
}

TEST_CASE("font decodes from bytes", "[scripting]")
{
  ScriptingTest vm(
      R"(
        local font = Font.decode(fontBytes)
        assert(font and font.ascent < 0)
    )",
      0,
      false,
      {},
      false);
  lua_State* L = vm.state();
  auto bytes = readFile(kLatin);
  setGlobal(L, "fontBytes", [&] {
    memcpy(lua_newbuffer(L, bytes.size()), bytes.data(), bytes.size());
  });
  vm.execute();
}

TEST_CASE("glyphs expose their placed outline", "[scripting]")
{
  TextTest vm(R"(
        local text = Text.new()
        local paint = Paint.with({ color = 0xFF0000FF })
        text:append('ab', { font = latin, size = 30, paint = paint })
        text:append('ج', { font = arabic, size = 30, paint = paint })
        function render(renderer: Renderer)
            for _, line in text:lines() do
                for _, glyph in line do
                    renderer:drawPath(glyph:path(), paint)
                    renderer:save()
                    renderer:transform(Mat2D.withTranslation(0, 40) * glyph.transform)
                    renderer:drawPath(glyph.font:glyphPath(glyph.id), paint)
                    renderer:restore()
                end
            end
        end
    )");
  drawWith(vm, "render");
  CHECK(vm.serializer()->matches("scripted_text_glyphs"));
}
