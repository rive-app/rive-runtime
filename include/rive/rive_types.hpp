/*
 * Copyright 2022 Rive
 */

// This should always be included by any other rive files,
// as it performs basic self-consistency checks, and provides
// shared common types and macros.

#ifndef _RIVE_TYPES_HPP_
#define _RIVE_TYPES_HPP_

#include <memory>   // For unique_ptr.
#include <string.h> // For memcpy.

#if defined(DEBUG) && defined(NDEBUG)
#error "can't determine if we're debug or release"
#endif

#if !defined(DEBUG) && !defined(NDEBUG)
// we have to make a decision what mode we're in
// historically this has been to look for NDEBUG, and in its
// absence assume we're DEBUG.
#define DEBUG 1
// fyi - Xcode seems to set DEBUG (or not), so the above guess
// doesn't work for them - so our projects may need to explicitly
// set NDEBUG in our 'release' builds.
#endif

#ifdef NDEBUG
#ifndef RELEASE
#define RELEASE 1
#endif
#else // debug mode
#ifndef DEBUG
#define DEBUG 1
#endif
#endif

// Some checks to guess what platform we're building for

#ifdef __APPLE__

#define RIVE_BUILD_FOR_APPLE
#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#define RIVE_BUILD_FOR_IOS
#elif TARGET_OS_MAC
#define RIVE_BUILD_FOR_OSX
#endif

#endif

// We really like these headers, so we include them all the time.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

// Annotations to assert unreachable control flow.
#if defined(__GNUC__) || defined(__clang__)
#define RIVE_UNREACHABLE                                                                           \
    assert(!(bool)"unreachable reached");                                                          \
    __builtin_unreachable
#elif _MSC_VER
#define RIVE_UNREACHABLE()                                                                         \
    assert(!(bool)"unreachable reached");                                                          \
    __assume(0)
#else
#define RIVE_UNREACHABLE()                                                                         \
    do                                                                                             \
    {                                                                                              \
        assert(!(bool)"unreachable reached");                                                      \
    } while (0)
#endif

#if __cplusplus >= 201703L
#define RIVE_MAYBE_UNUSED [[maybe_unused]]
#else
#define RIVE_MAYBE_UNUSED
#endif

#if __cplusplus >= 201703L
#define RIVE_FALLTHROUGH [[fallthrough]]
#elif defined(__clang__)
#define RIVE_FALLTHROUGH [[clang::fallthrough]]
#else
#define RIVE_FALLTHROUGH
#endif

#if defined(__GNUC__) || defined(__clang__)
#define RIVE_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define RIVE_ALWAYS_INLINE inline
#endif

#if defined(__GNUC__) || defined(__clang__)
// Recommended in https://clang.llvm.org/docs/LanguageExtensions.html#feature-checking-macros
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif
#else
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_memcpy)
#define RIVE_INLINE_MEMCPY __builtin_memcpy
#else
#define RIVE_INLINE_MEMCPY memcpy
#endif

#ifdef DEBUG
#define RIVE_DEBUG_CODE(CODE) CODE
#else
#define RIVE_DEBUG_CODE(CODE)
#endif

// Backports of later stl functions.
namespace rivestd
{
template <class T, class... Args> std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
} // namespace rivestd

#endif // rive_types
