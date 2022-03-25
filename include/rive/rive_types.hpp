/*
 * Copyright 2022 Rive
 */

// This should always be included by any other rive files,
// as it performs basic self-consistency checks, and provides
// shared common types and macros.

#ifndef _RIVE_TYPES_HPP_
#define _RIVE_TYPES_HPP_

// We treat NDEBUG is the root of truth from the user.
// Our other symbols (DEBUG, RELEASE) must agree with it.
// ... its ok if our client doesn't set these, we will do that...
// ... they just can't set them inconsistently.

#ifdef NDEBUG
// we're in release mode
#ifdef DEBUG
#error "can't define both DEBUG and NDEBUG"
#endif
#ifndef RELEASE
#define RELEASE
#endif
#else
// we're in debug mode
#ifdef RELEASE
#error "can't define RELEASE and not NDEBUG"
#endif
#ifndef DEBUG
#define DEBUG
#endif
#endif

// We really like these headers, so we include them all the time.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

#endif // rive_types
