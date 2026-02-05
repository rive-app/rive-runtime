#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/file.hpp"
#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/component.hpp"
#include "rive/artboard.hpp"
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace rive;

#ifdef WITH_RIVE_AUDIO
void ScriptedAudioSource::source(rcp<AudioSource> value) { m_source = value; }

static int audio_engine_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto scriptedAudioEngine = lua_torive<ScriptedAudioEngine>(L, 1);
        auto engine = scriptedAudioEngine->engine();
        auto* artboard = scriptedAudioEngine->artboard();
        if (engine == nullptr)
        {
            return 0;
        }
        switch (atom)
        {
            case (int)LuaAtoms::play:
            {
                auto* scriptedAudioSource =
                    lua_torive<ScriptedAudioSource>(L, 2);
                if (scriptedAudioSource == nullptr ||
                    scriptedAudioSource->source() == nullptr)
                {
                    return 0;
                }
                uint64_t startTime = engine->timeInFrames();
                rcp<AudioSound> sound =
                    engine->play(scriptedAudioSource->source(),
                                 startTime,
                                 0,
                                 0,
                                 artboard);
                if (sound != nullptr)
                {
                    if (artboard)
                    {
                        sound->volume(artboard->volume());
                    }
                    auto* scriptedAudioSound =
                        lua_newrive<ScriptedAudioSound>(L, artboard);
                    scriptedAudioSound->sound = std::move(sound);
                    return 1;
                }
                return 0;
            }
            case (int)LuaAtoms::stop:
            {
                if (artboard != nullptr)
                {
                    engine->stop(artboard);
                }
                return 0;
            }
            default:
                break;
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedAudioEngine::luaName);
    return 0;
}

static int audio_sound_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto* scriptedAudioSound = lua_torive<ScriptedAudioSound>(L, 1);
        if (scriptedAudioSound == nullptr ||
            scriptedAudioSound->sound == nullptr)
        {
            return 0;
        }
        auto sound = scriptedAudioSound->sound;
        switch (atom)
        {
            case (int)LuaAtoms::stop:
            {
                uint64_t fadeTimeInFrames = 0;
                if (lua_gettop(L) >= 2 && lua_isnumber(L, 2))
                {
                    fadeTimeInFrames = (uint64_t)lua_tonumber(L, 2);
                }
                sound->stop(fadeTimeInFrames);
                return 0;
            }
            case (int)LuaAtoms::seek:
            {
                float timeInSeconds = (float)luaL_checknumber(L, 2);
                lua_pushboolean(L, sound->seekSeconds(timeInSeconds));
                return 1;
            }
            case (int)LuaAtoms::seekFrame:
            {
                uint64_t timeInFrames = (uint64_t)luaL_checknumber(L, 2);
                lua_pushboolean(L, sound->seek(timeInFrames));
                return 1;
            }
            case (int)LuaAtoms::completed:
            {
                lua_pushboolean(L, sound->completed());
                return 1;
            }
            case (int)LuaAtoms::time:
            {
                lua_pushnumber(L, sound->timeInSeconds());
                return 1;
            }
            case (int)LuaAtoms::timeFrame:
            {
                lua_pushnumber(L, sound->timeInFrames());
                return 1;
            }
            default:
                break;
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedAudioSound::luaName);
    return 0;
}

static int audio_sound_newindex(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedAudioSound = lua_torive<ScriptedAudioSound>(L, 1);
    auto sound = scriptedAudioSound->sound;
    auto artboard = scriptedAudioSound->artboard();
    switch (atom)
    {
        case (int)LuaAtoms::volume:
        {
            float value = (float)luaL_checknumber(L, 3);
            if (artboard)
            {
                value *= artboard->volume();
            }
            sound->volume(value);
            return 0;
        }
        default:
            break;
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedAudioSound::luaName);

    return 0;
}

static int audio_sound_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedAudioSound = lua_torive<ScriptedAudioSound>(L, 1);
    auto sound = scriptedAudioSound->sound;
    switch (atom)
    {
        case (int)LuaAtoms::volume:
            lua_pushnumber(L, (lua_Number)sound->volume());
            return 1;

        default:
            break;
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedAudioSound::luaName);
    return 0;
}
#endif

int luaopen_rive_audio(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    {
        lua_register_rive<ScriptedAudioEngine>(L);

        lua_pushcfunction(L, audio_engine_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    {
        lua_register_rive<ScriptedAudioSource>(L);

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    {
        lua_register_rive<ScriptedAudioSound>(L);

        lua_pushcfunction(L, audio_sound_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_pushcfunction(L, audio_sound_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, audio_sound_newindex, nullptr);
        lua_setfield(L, -2, "__newindex");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
#endif

    return 0;
}

#endif
