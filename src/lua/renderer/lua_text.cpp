#if defined(WITH_RIVE_SCRIPTING) && defined(WITH_RIVE_TEXT)
#include "lua.h"
#include "lualib.h"
#include "rive/lua/scripted_text.hpp"
#include "rive/text/cursor.hpp"
#include "rive/factory.hpp"

using namespace rive;

namespace
{
struct AtomName
{
    LuaAtoms atom;
    const char* name;
};

// Rows follow the runtime enum order so the index is the enum value.
constexpr AtomName sizingNames[] = {{LuaAtoms::autoWidth, "autoWidth"},
                                    {LuaAtoms::autoHeight, "autoHeight"},
                                    {LuaAtoms::fixed, "fixed"}};
constexpr AtomName overflowNames[] = {{LuaAtoms::visible, "visible"},
                                      {LuaAtoms::hidden, "hidden"},
                                      {LuaAtoms::clipped, "clipped"},
                                      {LuaAtoms::ellipsis, "ellipsis"}};
// left, right, center map to TextAlign; start and end resolve against the
// paragraph direction.
constexpr AtomName alignNames[] = {{LuaAtoms::left, "left"},
                                   {LuaAtoms::right, "right"},
                                   {LuaAtoms::center, "center"},
                                   {LuaAtoms::start, "start"},
                                   {LuaAtoms::end, "end"}};
constexpr AtomName wrapNames[] = {{LuaAtoms::wrap, "wrap"},
                                  {LuaAtoms::noWrap, "noWrap"}};
constexpr AtomName wordBreakNames[] = {{LuaAtoms::breakWord, "breakWord"},
                                       {LuaAtoms::normal, "normal"},
                                       {LuaAtoms::breakAll, "breakAll"}};
constexpr AtomName originNames[] = {{LuaAtoms::top, "top"},
                                    {LuaAtoms::baseline, "baseline"}};
// Index minus one is the shaper's direction flag.
constexpr AtomName directionNames[] = {{LuaAtoms::autoDetect, "auto"},
                                       {LuaAtoms::ltr, "ltr"},
                                       {LuaAtoms::rtl, "rtl"}};

template <size_t N>
int readEnum(lua_State* L,
             int idx,
             const AtomName (&names)[N],
             const char* typeName)
{
    int atom;
    const char* str = lua_tostringatom(L, idx, &atom);
    if (!str)
    {
        luaL_typeerrorL(L, idx, lua_typename(L, LUA_TSTRING));
    }
    for (size_t i = 0; i < N; i++)
    {
        if ((int)names[i].atom == atom)
        {
            return (int)i;
        }
    }
    luaL_error(L, "'%s' is not a valid %s", str, typeName);
    return 0;
}

template <size_t N>
int pushEnum(lua_State* L, const AtomName (&names)[N], int value)
{
    lua_pushstring(L, names[value].name);
    return 1;
}

int pushDirection(lua_State* L, TextDirection direction)
{
    return pushEnum(L, directionNames, (int)direction + 1);
}

ScriptedTextLayout* checkLayout(lua_State* L, int idx)
{
    return lua_torive<ScriptedText>(L, idx)->layout.get();
}

void checkFresh(lua_State* L, ScriptedTextLayout* layout, uint32_t revision)
{
    if (layout->text.revision() != revision)
    {
        luaL_error(L, "Text changed since this was fetched, fetch it again.");
    }
}

const OrderedLine& checkLine(lua_State* L, ScriptedTextLine* line)
{
    checkFresh(L, line->layout.get(), line->revision);
    return line->layout->text.orderedLines()[line->index];
}

void pushLine(lua_State* L, ScriptedText* text, uint32_t index)
{
    auto line = lua_newrive<ScriptedTextLine>(L);
    line->layout = text->layout;
    line->revision = text->layout->text.revision();
    line->index = index;
}

void pushGlyph(lua_State* L,
               ScriptedTextLine* line,
               const GlyphRun& run,
               uint32_t glyphIndex,
               Vec2D position)
{
    auto glyph = lua_newrive<ScriptedGlyph>(L);
    glyph->layout = line->layout;
    glyph->revision = line->revision;
    glyph->run = &run;
    glyph->glyphIndex = glyphIndex;
    glyph->position = position;
}

Mat2D glyphTransform(const ScriptedGlyph* glyph)
{
    return RawText::glyphTransform(*glyph->run, glyph->position);
}

// Text

int text_new(lua_State* L)
{
    auto context = static_cast<ScriptingContext*>(lua_getthreaddata(L));
    lua_newrive<ScriptedText>(L)->layout =
        make_rcp<ScriptedTextLayout>(context->factory());
    return 1;
}

// Reads one field of an append style table, leaving the stack as it was.
template <typename F>
void withField(lua_State* L, int table, const char* name, F f)
{
    lua_rawgetfield(L, table, name);
    if (!lua_isnil(L, -1))
    {
        f(lua_gettop(L));
    }
    lua_pop(L, 1);
}

int text_append(lua_State* L)
{
    RawText& text = checkLayout(L, 1)->text;
    size_t length;
    const char* chars = luaL_checklstring(L, 2, &length);
    luaL_checktype(L, 3, LUA_TTABLE);

    rcp<Font> font;
    rcp<RenderPaint> paint;
    float size = 16.0f, lineHeight = -1.0f, letterSpacing = 0.0f;
    ColorInt foreground = 0xFF000000;
    withField(L, 3, "font", [&](int i) {
        font = lua_torive<ScriptedFont>(L, i)->font;
    });
    withField(L, 3, "paint", [&](int i) {
        paint = lua_torive<ScriptedPaint>(L, i)->renderPaint;
    });
    withField(L, 3, "size", [&](int i) {
        size = (float)luaL_checknumber(L, i);
    });
    withField(L, 3, "lineHeight", [&](int i) {
        lineHeight = (float)luaL_checknumber(L, i);
    });
    withField(L, 3, "letterSpacing", [&](int i) {
        letterSpacing = (float)luaL_checknumber(L, i);
    });
    withField(L, 3, "foregroundColor", [&](int i) {
        foreground = luaL_checkunsigned(L, i);
    });
    if (font == nullptr)
    {
        luaL_error(L, "Text:append needs a font.");
    }
    text.append(std::string(chars, length),
                std::move(paint),
                std::move(font),
                size,
                lineHeight,
                letterSpacing,
                foreground);
    return 0;
}

int text_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    auto scriptedText = lua_torive<ScriptedText>(L, 1);
    RawText& text = scriptedText->layout->text;
    switch (atom)
    {
        case (int)LuaAtoms::append:
            return text_append(L);
        case (int)LuaAtoms::clear:
            text.clear();
            return 0;
        case (int)LuaAtoms::draw:
        {
            auto renderer = lua_torive<ScriptedRenderer>(L, 2)->validate(L);
            auto paint = lua_torive<ScriptedPaint>(L, 3, true);
            text.render(renderer, paint ? paint->renderPaint : nullptr);
            return 0;
        }
        case (int)LuaAtoms::bounds:
        {
            AABB bounds = text.bounds();
            lua_pushvec2d(L, Vec2D(bounds.minX, bounds.minY));
            lua_pushvec2d(L, Vec2D(bounds.maxX, bounds.maxY));
            return 2;
        }
        case (int)LuaAtoms::line:
        {
            int index = luaL_checkinteger(L, 2);
            if (index < 1 || (size_t)index > text.orderedLines().size())
            {
                luaL_error(L, "line %d is out of range", index);
            }
            pushLine(L, scriptedText, (uint32_t)index - 1);
            return 1;
        }
        case (int)LuaAtoms::lines:
        {
            size_t count = text.orderedLines().size();
            lua_createtable(L, (int)count, 0);
            for (size_t i = 0; i < count; i++)
            {
                pushLine(L, scriptedText, (uint32_t)i);
                lua_rawseti(L, -2, (int)i + 1);
            }
            return 1;
        }
        case (int)LuaAtoms::hitTest:
        {
            const Vec2D* position = lua_checkvec2d(L, 2);
            TextLayoutView view = text.layoutView();
            lua_pushinteger(L,
                            CursorPosition::fromTranslation(*position, view)
                                    .codePointIndex() +
                                1);
            return 1;
        }
        case (int)LuaAtoms::caret:
        {
            int index = luaL_checkinteger(L, 2);
            TextLayoutView view = text.layoutView();
            auto visual =
                CursorPosition::atIndex(index < 1 ? 0 : index - 1, view)
                    .visualPosition(view);
            if (!visual.found())
            {
                return 0;
            }
            lua_pushnumber(L, visual.x());
            lua_pushnumber(L, visual.top());
            lua_pushnumber(L, visual.bottom());
            return 3;
        }
        case (int)LuaAtoms::selectionRects:
        {
            int from = luaL_checkinteger(L, 2);
            int to = luaL_checkinteger(L, 3);
            TextLayoutView view = text.layoutView();
            Cursor cursor(
                CursorPosition::atIndex(from < 1 ? 0 : from - 1, view),
                CursorPosition::atIndex(to < 1 ? 0 : to - 1, view));
            std::vector<AABB> rects;
            cursor.selectionRects(rects, view);
            lua_createtable(L, (int)rects.size(), 0);
            int i = 1;
            for (const AABB& rect : rects)
            {
                lua_createtable(L, 0, 2);
                lua_pushvec2d(L, Vec2D(rect.minX, rect.minY));
                lua_setfield(L, -2, "min");
                lua_pushvec2d(L, Vec2D(rect.maxX, rect.maxY));
                lua_setfield(L, -2, "max");
                lua_rawseti(L, -2, i++);
            }
            return 1;
        }
    }
    luaL_error(L, "%s is not a valid method of %s", str, ScriptedText::luaName);
    return 0;
}

int text_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
    }
    RawText& text = checkLayout(L, 1)->text;
    switch (atom)
    {
        case (int)LuaAtoms::sizing:
            return pushEnum(L, sizingNames, (int)text.sizing());
        case (int)LuaAtoms::overflow:
            return pushEnum(L, overflowNames, (int)text.overflow());
        case (int)LuaAtoms::align:
            return pushEnum(L, alignNames, text.alignIndex());
        case (int)LuaAtoms::wrap:
            return pushEnum(L, wrapNames, (int)text.wrap());
        case (int)LuaAtoms::wordBreak:
            return pushEnum(L, wordBreakNames, (int)text.wordBreak());
        case (int)LuaAtoms::origin:
            return pushEnum(L, originNames, (int)text.origin());
        case (int)LuaAtoms::direction:
            return pushEnum(L, directionNames, text.directionIndex());
        case (int)LuaAtoms::maxWidth:
            lua_pushnumber(L, text.maxWidth());
            return 1;
        case (int)LuaAtoms::maxHeight:
            lua_pushnumber(L, text.maxHeight());
            return 1;
        case (int)LuaAtoms::paragraphSpacing:
            lua_pushnumber(L, text.paragraphSpacing());
            return 1;
        case (int)LuaAtoms::length:
            lua_pushnumber(L, (double)text.length());
            return 1;
        case (int)LuaAtoms::isEmpty:
            lua_pushboolean(L, text.empty());
            return 1;
        case (int)LuaAtoms::lineCount:
            lua_pushnumber(L, (double)text.orderedLines().size());
            return 1;
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedText::luaName);
    return 0;
}

int text_newindex(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
    }
    RawText& text = checkLayout(L, 1)->text;
    switch (atom)
    {
        case (int)LuaAtoms::sizing:
            text.sizingIndex(readEnum(L, 3, sizingNames, "TextSizing"));
            return 0;
        case (int)LuaAtoms::overflow:
            text.overflowIndex(readEnum(L, 3, overflowNames, "TextOverflow"));
            return 0;
        case (int)LuaAtoms::align:
            text.alignIndex(readEnum(L, 3, alignNames, "TextAlign"));
            return 0;
        case (int)LuaAtoms::wrap:
            text.wrapIndex(readEnum(L, 3, wrapNames, "TextWrap"));
            return 0;
        case (int)LuaAtoms::wordBreak:
            text.wordBreakIndex(
                readEnum(L, 3, wordBreakNames, "TextWordBreak"));
            return 0;
        case (int)LuaAtoms::origin:
            text.originIndex(readEnum(L, 3, originNames, "TextOrigin"));
            return 0;
        case (int)LuaAtoms::direction:
            text.directionIndex(
                readEnum(L, 3, directionNames, "TextDirection"));
            return 0;
        case (int)LuaAtoms::maxWidth:
            text.maxWidth((float)luaL_checknumber(L, 3));
            return 0;
        case (int)LuaAtoms::maxHeight:
            text.maxHeight((float)luaL_checknumber(L, 3));
            return 0;
        case (int)LuaAtoms::paragraphSpacing:
            text.paragraphSpacing((float)luaL_checknumber(L, 3));
            return 0;
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedText::luaName);
    return 0;
}

// TextLine

int line_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
    }
    auto line = lua_torive<ScriptedTextLine>(L, 1);
    const OrderedLine& orderedLine = checkLine(L, line);
    const GlyphLine& glyphLine = orderedLine.glyphLine();
    switch (atom)
    {
        case (int)LuaAtoms::index:
            lua_pushinteger(L, line->index + 1);
            return 1;
        case (int)LuaAtoms::x:
            lua_pushnumber(L, glyphLine.startX);
            return 1;
        case (int)LuaAtoms::top:
            lua_pushnumber(L, line->layout->text.lineTop(line->index));
            return 1;
        case (int)LuaAtoms::baseline:
            lua_pushnumber(L, orderedLine.y());
            return 1;
        case (int)LuaAtoms::bottom:
            lua_pushnumber(L, orderedLine.bottom());
            return 1;
        case (int)LuaAtoms::width:
        {
            float width = 0.0f;
            line->layout->text.forEachGlyph(
                line->index,
                [&](const GlyphRun& run, uint32_t glyphIndex, Vec2D) {
                    width += run.advances[glyphIndex];
                    return true;
                });
            lua_pushnumber(L, width);
            return 1;
        }
        case (int)LuaAtoms::direction:
            return pushDirection(L,
                                 line->layout->text.lineDirection(line->index));
        case (int)LuaAtoms::firstIndex:
            lua_pushinteger(L,
                            orderedLine.firstCodePointIndex(
                                line->layout->text.layoutView().glyphLookup()) +
                                1);
            return 1;
        case (int)LuaAtoms::lastIndex:
            lua_pushinteger(L,
                            orderedLine.lastCodePointIndex(
                                line->layout->text.layoutView().glyphLookup()) +
                                1);
            return 1;
        case (int)LuaAtoms::glyphCount:
            lua_pushinteger(L, line->layout->text.lineGlyphCount(line->index));
            return 1;
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedTextLine::luaName);
    return 0;
}

int line_iter_next(lua_State* L)
{
    auto cursor = lua_torive<ScriptedGlyphCursor>(L, 1);
    checkFresh(L, cursor->layout.get(), cursor->revision);
    if (cursor->itr == cursor->end)
    {
        lua_pushnil(L);
        return 1;
    }
    const GlyphRun* run = cursor->itr.run();
    uint32_t glyphIndex = cursor->itr.glyphIndex();
    const Vec2D& offset = run->offsets[glyphIndex];
    float y = cursor->layout->text.orderedLines()[cursor->line].y();
    lua_pushinteger(L, luaL_checkinteger(L, 2) + 1);
    auto glyph = lua_newrive<ScriptedGlyph>(L);
    glyph->layout = cursor->layout;
    glyph->revision = cursor->revision;
    glyph->run = run;
    glyph->glyphIndex = glyphIndex;
    glyph->position = Vec2D(cursor->x + offset.x, y + offset.y);
    cursor->x += run->advances[glyphIndex];
    ++cursor->itr;
    return 2;
}

int line_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    auto line = lua_torive<ScriptedTextLine>(L, 1);
    checkLine(L, line);
    switch (atom)
    {
        case (int)LuaAtoms::glyph:
        {
            int wanted = luaL_checkinteger(L, 2) - 1;
            int index = 0;
            bool pushed = false;
            line->layout->text.forEachGlyph(
                line->index,
                [&](const GlyphRun& run, uint32_t glyphIndex, Vec2D position) {
                    if (index++ != wanted)
                    {
                        return true;
                    }
                    pushGlyph(L, line, run, glyphIndex, position);
                    pushed = true;
                    return false;
                });
            if (!pushed)
            {
                luaL_error(L, "glyph %d is out of range", wanted + 1);
            }
            return 1;
        }
        case (int)LuaAtoms::glyphs:
        {
            int count = 0;
            lua_createtable(L,
                            (int)line->layout->text.lineGlyphCount(line->index),
                            0);
            line->layout->text.forEachGlyph(
                line->index,
                [&](const GlyphRun& run, uint32_t glyphIndex, Vec2D position) {
                    pushGlyph(L, line, run, glyphIndex, position);
                    lua_rawseti(L, -2, ++count);
                    return true;
                });
            return 1;
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedTextLine::luaName);
    return 0;
}

// for _, glyph in line do
int line_iter(lua_State* L)
{
    auto line = lua_torive<ScriptedTextLine>(L, 1);
    const OrderedLine& orderedLine = checkLine(L, line);
    lua_pushcfunction(L, line_iter_next, "TextLine.iter");
    auto cursor = lua_newrive<ScriptedGlyphCursor>(L);
    cursor->layout = line->layout;
    cursor->revision = line->revision;
    cursor->line = line->index;
    cursor->itr = orderedLine.begin();
    cursor->end = orderedLine.end();
    cursor->x = orderedLine.glyphLine().startX;
    lua_pushinteger(L, 0);
    return 3;
}

// Glyph

const ScriptedGlyph* checkGlyph(lua_State* L)
{
    auto glyph = lua_torive<ScriptedGlyph>(L, 1);
    checkFresh(L, glyph->layout.get(), glyph->revision);
    return glyph;
}

int glyph_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
    }
    const ScriptedGlyph* glyph = checkGlyph(L);
    const GlyphRun* run = glyph->run;
    switch (atom)
    {
        case (int)LuaAtoms::id:
            lua_pushinteger(L, run->glyphs[glyph->glyphIndex]);
            return 1;
        case (int)LuaAtoms::font:
            lua_pushfont(L, run->font);
            return 1;
        case (int)LuaAtoms::size:
            lua_pushnumber(L, run->size);
            return 1;
        case (int)LuaAtoms::x:
            lua_pushnumber(L, glyph->position.x);
            return 1;
        case (int)LuaAtoms::y:
            lua_pushnumber(L, glyph->position.y);
            return 1;
        case (int)LuaAtoms::advance:
            lua_pushnumber(L, run->advances[glyph->glyphIndex]);
            return 1;
        case (int)LuaAtoms::textIndex:
            lua_pushinteger(L, run->textIndices[glyph->glyphIndex] + 1);
            return 1;
        case (int)LuaAtoms::direction:
            return pushDirection(L, run->dir());
        case (int)LuaAtoms::isColor:
            lua_pushboolean(
                L,
                run->font->isColorGlyph(run->glyphs[glyph->glyphIndex]));
            return 1;
        case (int)LuaAtoms::transform:
            lua_newrive<ScriptedMat2D>(L, glyphTransform(glyph));
            return 1;
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedGlyph::luaName);
    return 0;
}

int glyph_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    const ScriptedGlyph* glyph = checkGlyph(L);
    const GlyphRun* run = glyph->run;
    switch (atom)
    {
        case (int)LuaAtoms::path:
        {
            Mat2D transform = glyphTransform(glyph);
            RawPath path;
            path.addPath(
                RawText::glyphPath(*run->font, run->glyphs[glyph->glyphIndex]),
                &transform);
            lua_newrive<ScriptedPath>(L, &path);
            return 1;
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedGlyph::luaName);
    return 0;
}

const luaL_Reg textStaticMethods[] = {
    {"new", text_new},
    {NULL, NULL},
};

} // namespace

int luaopen_rive_text(lua_State* L)
{
    luaL_register(L, ScriptedText::luaName, textStaticMethods);
    lua_register_rive_type<ScriptedText>(L,
                                         text_index,
                                         text_namecall,
                                         text_newindex,
                                         nullptr);
    lua_register_rive_type<ScriptedTextLine>(L,
                                             line_index,
                                             line_namecall,
                                             nullptr,
                                             line_iter);
    lua_register_rive_type<ScriptedGlyph>(L,
                                          glyph_index,
                                          glyph_namecall,
                                          nullptr,
                                          nullptr);
    lua_register_rive<ScriptedGlyphCursor>(L);
    return 1;
}
#elif defined(WITH_RIVE_SCRIPTING)
struct lua_State;
int luaopen_rive_text(lua_State*) { return 0; }
#endif
