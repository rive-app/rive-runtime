/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/async/threading.hpp"
#include "rive/rive_types.hpp"

#include <atomic>
#include <type_traits>

namespace rive
{

/// Same as std::atomic's compare exchanges.
#if defined(__wasm__) && !defined(__wasm_atomics__)
// Without wasm atomics no other thread shares memory, and at -Oz std's
// exchange stays out of line with a branch for every memory order.
#if defined(__EMSCRIPTEN__) && !RIVE_ONE_THREAD()
#error "a shared heap needs wasm atomics"
#endif
template <typename T, typename... Orders>
RIVE_ALWAYS_INLINE bool compareExchangeStrong(
    std::atomic<T>& word,
    T& expected,
    typename std::atomic<T>::value_type desired,
    Orders...)
{
  static_assert(std::has_unique_object_representations<T>::value, "");
  T current = word.load(std::memory_order_relaxed);
  if (current != expected)
  {
    expected = current;
    return false;
  }
  word.store(desired, std::memory_order_relaxed);
  return true;
}

template <typename T, typename... Orders>
RIVE_ALWAYS_INLINE bool compareExchangeWeak(
    std::atomic<T>& word,
    T& expected,
    typename std::atomic<T>::value_type desired,
    Orders... orders)
{
  return compareExchangeStrong(word, expected, desired, orders...);
}
#else
template <typename T, typename... Orders>
RIVE_ALWAYS_INLINE bool compareExchangeStrong(
    std::atomic<T>& word,
    T& expected,
    typename std::atomic<T>::value_type desired,
    Orders... orders)
{
  return word.compare_exchange_strong(expected, desired, orders...);
}

template <typename T, typename... Orders>
RIVE_ALWAYS_INLINE bool compareExchangeWeak(
    std::atomic<T>& word,
    T& expected,
    typename std::atomic<T>::value_type desired,
    Orders... orders)
{
  return word.compare_exchange_weak(expected, desired, orders...);
}
#endif

} // namespace rive
