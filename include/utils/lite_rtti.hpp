/*
 * Copyright 2023 Rive
 */

// "lite_rtti_cast<T*>()" is a very basic polyfill for "dynamic_cast<T*>()" that
// does not require all of the heavyweight C++ RTTI to be enabled in the
// compiler. The root of the RTTI chain must derive from enable_lite_rtti, and
// any subclasses must inherit from lite_rtti_override (using the provided
// macros)
//
//     class Root : public ENABLE_LITE_RTTI(Root) {};
//     class Derived : public LITE_RTTI_OVERRIDE(Root, Derived) {};
//     Root* derived = new Derived();
//     lite_rtti_cast<Derived*>(derived);
//

#pragma once

#include "utils/compile_time_string_hash.hpp"
#include "rive/refcnt.hpp"
#include <cassert>
#include <stdint.h>
#include <type_traits>

namespace rive
{
namespace detail
{
// This helper is called by lite_rtti_cast (and related functions) to check
// whether the cast is allowed.
template <typename Dst, typename Src> bool canRTTICastTo(Src* src)
{
  using D = std::remove_pointer_t<std::remove_reference_t<Dst>>;

  static_assert(
      std::is_base_of_v<Src, D> || std::is_base_of_v<D, Src>,
      "lite RTTI casts should only be used between related types");
  static_assert(
      std::is_base_of_v<Src, D>,
      "lite RTTI casts should only be used for downcasts, otherwise use static_cast");
  static_assert(
      !std::is_same_v<std::remove_const_t<Src>, std::remove_const_t<D>>,
      "lite RTTI casts don't need to be used for casting a type to itself");
  static_assert(
      std::is_const_v<D> || !std::is_const_v<Src>,
      "cannot remove const during a lite RTTI cast");
  return src != nullptr && src->canRTTICastTo(D::TypeIDForRTTI);
}
} // namespace detail

template <uint32_t ID> class enable_lite_rtti
{
private:
  // We don't allow casts from same type to same type so this will never return
  // true, as it's the root and can't be downcast to.
  virtual bool canRTTICastTo(uint32_t id) const { return false; }

  template <class, uint32_t> friend class lite_rtti_override;
  template <typename Dst, typename Src>
  friend bool detail::canRTTICastTo(Src* src);

#ifdef WITH_RIVE_TOOLS
public: // For unit tests, make this public
#endif
  static constexpr uint32_t TypeIDForRTTI = ID;
};

// lite_rtti_override is the class for any derived types.
template <class Base, uint32_t ID> class lite_rtti_override : public Base
{
public:
  // import all parent-class constructors
  using Base::Base;

  virtual ~lite_rtti_override() = default;

private:
  bool canRTTICastTo(uint32_t id) const override
  {
    return ID == id || Base::canRTTICastTo(id);
  }

  template <class, uint32_t> friend class lite_rtti_override;
  template <typename Dst, typename Src>
  friend bool detail::canRTTICastTo(Src* src);

#ifdef WITH_RIVE_TOOLS
public: // For unit tests, make this public
#endif
  static constexpr uint32_t TypeIDForRTTI = ID;
};

// Like dynamic_cast<>, but opt-in, without requiring C++ RTTI to be enabled.
template <class DstPtr, class Src> DstPtr lite_rtti_cast(Src* src)
{
  static_assert(
      std::is_pointer_v<DstPtr>,
      "lite_rtti_cast should be done to a pointer type");
  return detail::canRTTICastTo<DstPtr>(src) ? static_cast<DstPtr>(src)
                                            : nullptr;
}

// Like lite_rtti_cast<>, but asserts that the cast succeeded.
template <class DstPtr, class Src> DstPtr lite_rtti_cast_or_assert(Src* src)
{
  DstPtr dst = lite_rtti_cast<DstPtr>(src);
  assert(dst != nullptr);
  return dst;
}

template <class Dst, class Src> rcp<Dst> lite_rtti_rcp_cast(rcp<Src> src)
{
  return detail::canRTTICastTo<Dst>(src.get()) ? static_rcp_cast<Dst>(src)
                                               : nullptr;
}

// Different versions of clang-format disagree on how to format these.
// clang-format off

#define ENABLE_LITE_RTTI(ROOT) enable_lite_rtti<CONST_ID(ROOT)>
#define LITE_RTTI_OVERRIDE(BASE, DERIVED)                                      \
  lite_rtti_override<BASE, CONST_ID(DERIVED)>

#define LITE_RTTI_CAST_OR_RETURN(NAME, TYPE, POINTER)                          \
  auto NAME = rive::lite_rtti_cast<TYPE>(POINTER);                             \
  if (NAME == nullptr)                                                         \
    return

#define LITE_RTTI_CAST_OR_BREAK(NAME, TYPE, POINTER)                           \
  auto NAME = rive::lite_rtti_cast<TYPE>(POINTER);                             \
  if (NAME == nullptr)                                                         \
    break

#define LITE_RTTI_CAST_OR_CONTINUE(NAME, TYPE, POINTER)                        \
  auto NAME = rive::lite_rtti_cast<TYPE>(POINTER);                             \
  if (NAME == nullptr)                                                         \
    continue
// clang-format on
} // namespace rive
