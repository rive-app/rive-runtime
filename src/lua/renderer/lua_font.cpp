#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#include "lualib.h"
#include "rive/lua/rive_lua_libs.hpp"
#ifdef WITH_RIVE_TEXT
#include "rive/lua/scripted_text.hpp"
#include "rive/text/font_hb.hpp"
#endif

#include <vector>

using namespace rive;

// OpenType tags cross as their four letter names.
static uint32_t tagFromString(lua_State* L, int idx)
{
    size_t length;
    const char* name = luaL_checklstring(L, idx, &length);
    if (length != 4)
    {
        luaL_error(L, "'%s' is not a four letter tag", name);
    }
    return ((uint32_t)(uint8_t)name[0] << 24) |
           ((uint32_t)(uint8_t)name[1] << 16) |
           ((uint32_t)(uint8_t)name[2] << 8) | (uint32_t)(uint8_t)name[3];
}

static void pushTag(lua_State* L, uint32_t tag)
{
    char name[4] = {(char)(tag >> 24),
                    (char)(tag >> 16),
                    (char)(tag >> 8),
                    (char)tag};
    lua_pushlstring(L, name, 4);
}

static Font* checkFont(lua_State* L, int idx)
{
    Font* font = lua_torive<ScriptedFont>(L, idx)->font.get();
    if (font == nullptr)
    {
        luaL_error(L, "Font is empty.");
    }
    return font;
}

static void font_direct_ascent(void* udata, void* result)
{
    Font* font = ((ScriptedFont*)udata)->font.get();
    lua_userdatadirectfield_setnumber(result,
                                      font ? font->lineMetrics().ascent : 0.0);
}

static void font_direct_descent(void* udata, void* result)
{
    Font* font = ((ScriptedFont*)udata)->font.get();
    lua_userdatadirectfield_setnumber(result,
                                      font ? font->lineMetrics().descent : 0.0);
}

static int font_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
    }
    Font* font = checkFont(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::ascent:
            lua_pushnumber(L, font->lineMetrics().ascent);
            return 1;
        case (int)LuaAtoms::descent:
            lua_pushnumber(L, font->lineMetrics().descent);
            return 1;
        case (int)LuaAtoms::capHeight:
            lua_pushnumber(L, font->lineMetrics().capHeight);
            return 1;
        case (int)LuaAtoms::xHeight:
            lua_pushnumber(L, font->lineMetrics().xHeight);
            return 1;
        case (int)LuaAtoms::weight:
            lua_pushnumber(L, font->getWeight());
            return 1;
        case (int)LuaAtoms::isItalic:
            lua_pushboolean(L, font->isItalic());
            return 1;
        case (int)LuaAtoms::axes:
        {
            uint16_t count = font->getAxisCount();
            lua_createtable(L, count, 0);
            for (uint16_t i = 0; i < count; i++)
            {
                Font::Axis axis = font->getAxis(i);
                lua_createtable(L, 0, 4);
                pushTag(L, axis.tag);
                lua_setfield(L, -2, "tag");
                lua_pushnumber(L, axis.min);
                lua_setfield(L, -2, "min");
                lua_pushnumber(L, axis.def);
                lua_setfield(L, -2, "default");
                lua_pushnumber(L, axis.max);
                lua_setfield(L, -2, "max");
                lua_rawseti(L, -2, i + 1);
            }
            return 1;
        }
        case (int)LuaAtoms::features:
        {
            SimpleArray<uint32_t> features = font->features();
            lua_createtable(L, (int)features.size(), 0);
            for (size_t i = 0; i < features.size(); i++)
            {
                pushTag(L, features[i]);
                lua_rawseti(L, -2, (int)i + 1);
            }
            return 1;
        }
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedFont::luaName);
    return 0;
}

// Reads { tag = value } pairs into coords or features.
template <typename T, typename Value>
static void readTagTable(lua_State* L, int idx, std::vector<T>& out)
{
    if (lua_isnoneornil(L, idx))
    {
        return;
    }
    luaL_checktype(L, idx, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, idx))
    {
        // Converting a numeric key in place would break the traversal.
        if (lua_type(L, -2) != LUA_TSTRING)
        {
            luaL_error(L, "font tags are four letter strings");
        }
        out.push_back({tagFromString(L, -2), (Value)luaL_checknumber(L, -1)});
        lua_pop(L, 1);
    }
}

static int font_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    Font* font = checkFont(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::lineHeight:
        {
            float size = (float)luaL_checknumber(L, 2);
            lua_pushnumber(L, font->descent(size) - font->ascent(size));
            return 1;
        }
        case (int)LuaAtoms::hasGlyph:
            lua_pushboolean(L,
                            font->hasGlyph((Unichar)luaL_checkunsigned(L, 2)));
            return 1;
        case (int)LuaAtoms::axisValue:
            lua_pushnumber(L, font->getAxisValue(tagFromString(L, 2)));
            return 1;
        case (int)LuaAtoms::withOptions:
        {
            std::vector<Font::Coord> coords;
            std::vector<Font::Feature> features;
            readTagTable<Font::Coord, float>(L, 2, coords);
            readTagTable<Font::Feature, uint32_t>(L, 3, features);
            lua_pushfont(L, font->withOptions(coords, features));
            return 1;
        }
#ifdef WITH_RIVE_TEXT
        case (int)LuaAtoms::glyphPath:
        {
            RawPath path =
                RawText::glyphPath(*font, (GlyphID)luaL_checkunsigned(L, 2));
            lua_newrive<ScriptedPath>(L, &path);
            return 1;
        }
#endif
    }
    luaL_error(L, "%s is not a valid method of %s", str, ScriptedFont::luaName);
    return 0;
}

#ifdef WITH_RIVE_TEXT
static int font_decode(lua_State* L)
{
    size_t length;
    void* bytes = luaL_checkbuffer(L, 1, &length);
    rcp<Font> font =
        HBFont::Decode(Span<const uint8_t>((const uint8_t*)bytes, length));
    if (font == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushfont(L, std::move(font));
    return 1;
}
#endif

static const luaL_Reg fontStaticMethods[] = {
#ifdef WITH_RIVE_TEXT
    {"decode", font_decode},
#endif
    {NULL, NULL},
};

int luaopen_rive_font(lua_State* L)
{
    luaL_register(L, ScriptedFont::luaName, fontStaticMethods);
    lua_register_rive_type<ScriptedFont>(L, font_index, font_namecall);

    lua_registeruserdatadirectfieldget(L,
                                       ScriptedFont::luaTag,
                                       "ascent",
                                       font_direct_ascent);
    lua_registeruserdatadirectfieldget(L,
                                       ScriptedFont::luaTag,
                                       "descent",
                                       font_direct_descent);
    return 1;
}
#endif
