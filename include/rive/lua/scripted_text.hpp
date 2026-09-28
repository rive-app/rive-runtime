#ifndef _RIVE_SCRIPTED_TEXT_HPP_
#define _RIVE_SCRIPTED_TEXT_HPP_

#if defined(WITH_RIVE_SCRIPTING) && defined(WITH_RIVE_TEXT)
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/text/raw_text.hpp"

namespace rive
{
// Shared by the Text userdata and the lines and glyphs it hands out, so those
// outlive a collected Text and can tell when the layout changed under them.
class ScriptedTextLayout : public RefCnt<ScriptedTextLayout>
{
public:
    ScriptedTextLayout(Factory* factory) : text(factory) {}
    RawText text;
};

class ScriptedText
{
public:
    rcp<ScriptedTextLayout> layout;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 71;
    static constexpr const char* luaName = "Text";
    static constexpr bool hasMetatable = true;
};

class ScriptedTextLine
{
public:
    rcp<ScriptedTextLayout> layout;
    uint32_t revision;
    uint32_t index;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 31;
    static constexpr const char* luaName = "TextLine";
    static constexpr bool hasMetatable = true;
};

// The generic for state over a line: `for _, glyph in line do` advances it
// one glyph per step instead of re-walking the line.
class ScriptedGlyphCursor
{
public:
    rcp<ScriptedTextLayout> layout;
    uint32_t revision;
    uint32_t line;
    GlyphItr itr;
    GlyphItr end;
    float x;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 72;
    static constexpr const char* luaName = "GlyphCursor";
    static constexpr bool hasMetatable = false;
};

class ScriptedGlyph
{
public:
    rcp<ScriptedTextLayout> layout;
    uint32_t revision;
    const GlyphRun* run;
    uint32_t glyphIndex;
    // Pen position on the baseline, the shaper's offset included.
    Vec2D position;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 37;
    static constexpr const char* luaName = "Glyph";
    static constexpr bool hasMetatable = true;
};
} // namespace rive
#endif
#endif
