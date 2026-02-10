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

int ScriptedAudioSource::initializeSound(lua_State* L,
                                         rcp<AudioSound> sound,
                                         Artboard* artboard)
{
    if (sound != nullptr)
    {
        if (artboard)
        {
            sound->volume(artboard->volume());
        }
        auto* scriptedAudioSound = lua_newrive<ScriptedAudioSound>(L, artboard);
        scriptedAudioSound->sound = std::move(sound);
        return 1;
    }
    return 0;
}

int ScriptedAudioSource::play(lua_State* L,
                              AudioEngine* engine,
                              double time,
                              bool isRelative)
{
    if (source() == nullptr)
    {
        return 0;
    }
    float startTime = time;
    if (isRelative)
    {
        startTime += engine->timeInSeconds();
    }
    rcp<AudioSound> audioSound =
        engine->playSeconds(source(), startTime, 0, 0, nullptr);
    return initializeSound(L, audioSound, nullptr);
}

int ScriptedAudioSource::play(lua_State* L, AudioEngine* engine)
{
    return play(L, engine, 0, true);
}
int ScriptedAudioSource::playFrame(lua_State* L,
                                   AudioEngine* engine,
                                   uint64_t startTime,
                                   bool isRelative)
{
    if (source() == nullptr)
    {
        return 0;
    }
    if (isRelative)
    {
        startTime += engine->timeInFrames();
    }
    rcp<AudioSound> audioSound =
        engine->play(source(), startTime, 0, 0, nullptr);
    return initializeSound(L, audioSound, nullptr);
}
int ScriptedAudioSource::playFrame(lua_State* L, AudioEngine* engine)
{
    return playFrame(L, engine, 0, true);
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

static int audio_play(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    if (engine == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto* scriptedAudioSource = lua_torive<ScriptedAudioSource>(L, 1);
    if (scriptedAudioSource == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    return scriptedAudioSource->play(L, engine.get());
#else
    lua_pushnil(L);
    return 1;
#endif
}

static int audio_play_at_time(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    if (engine == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto* scriptedAudioSource = lua_torive<ScriptedAudioSource>(L, 1);
    if (scriptedAudioSource == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto startTime = lua_tonumber(L, 2);

    return scriptedAudioSource->play(L, engine.get(), startTime, false);
#else
    lua_pushnil(L);
    return 1;
#endif
}

static int audio_play_in_time(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    if (engine == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto* scriptedAudioSource = lua_torive<ScriptedAudioSource>(L, 1);
    if (scriptedAudioSource == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto startTime = lua_tonumber(L, 2);

    return scriptedAudioSource->play(L, engine.get(), startTime, true);
#else
    lua_pushnil(L);
    return 1;
#endif
}

static int audio_play_at_frame(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    if (engine == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto* scriptedAudioSource = lua_torive<ScriptedAudioSource>(L, 1);
    if (scriptedAudioSource == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto startTime = lua_tonumber(L, 2);

    return scriptedAudioSource->playFrame(L, engine.get(), startTime, false);
#else
    lua_pushnil(L);
    return 1;
#endif
}

static int audio_play_in_frame(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    if (engine == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto* scriptedAudioSource = lua_torive<ScriptedAudioSource>(L, 1);
    if (scriptedAudioSource == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }
    auto startTime = lua_tonumber(L, 2);

    return scriptedAudioSource->playFrame(L, engine.get(), startTime, true);
#else
    lua_pushnil(L);
    return 1;
#endif
}

static int audio_time(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    lua_pushnumber(L, (lua_Number)(engine ? engine->timeInSeconds() : 0));
#else
    lua_pushnumber(L, 0);
#endif
    return 1;
}

static int audio_time_frame(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    lua_pushinteger(L, (engine ? (int)engine->timeInFrames() : 0));
#else
    lua_pushinteger(L, 0);
#endif
    return 1;
}

static int audio_sample_rate(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    lua_pushinteger(L, (engine ? (int)engine->sampleRate() : 0));
#else
    lua_pushinteger(L, 0);
#endif
    return 1;
}

static const luaL_Reg audioStaticMethods[] = {
    {"time", audio_time},
    {"timeFrame", audio_time_frame},
    {"sampleRate", audio_sample_rate},
    {"play", audio_play},
    {"playAtTime", audio_play_at_time},
    {"playInTime", audio_play_in_time},
    {"playAtFrame", audio_play_at_frame},
    {"playInFrame", audio_play_in_frame},
    {nullptr, nullptr}};
#endif

int luaopen_rive_audio(lua_State* L)
{
#ifdef WITH_RIVE_AUDIO
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
    luaL_register(L, ScriptedAudio::luaName, audioStaticMethods);
#endif

    return 0;
}

#endif
