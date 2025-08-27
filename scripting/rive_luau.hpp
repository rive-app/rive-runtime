#ifndef _RIVE_LUAU_HPP_
#define _RIVE_LUAU_HPP_

#ifdef WITH_RIVE_TOOLS
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define LUA_API extern "C" EMSCRIPTEN_KEEPALIVE
#elif defined(_MSC_VER)
#define LUA_API extern "C" __declspec(dllexport)
#else
#define LUA_API                                                                \
    extern "C" __attribute__((visibility("default"))) __attribute__((used))
#endif
#endif
#endif