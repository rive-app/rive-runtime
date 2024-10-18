/*
 * Copyright 2023 Rive
 */

// "lite_rtti_cast<T*>()" is a very basic polyfill for "dynamic_cast<T*>()" that
// can only cast a pointer to its most-derived type. To use it, the base class
// must derive from enable_lite_rtti, and the subclass must inherit from
// lite_rtti_override:
//
//     class Root : public enable_lite_rtti<Root> {};
//     class Derived : public lite_rtti_override<Root, Derived> {};
//     Root* derived = new Derived();
//     lite_rtti_cast<Derived*>(derived);
//

#pragma once

#include "utils/compile_time_string_hash.hpp"
#include "rive/refcnt.hpp"
#include <stdint.h>
#include <type_traits>

namespace rive
{

// Enable lite rtti on the root of a class hierarchy.
template <class Root, unsigned int ID> class enable_lite_rtti
{
public:
    unsigned int liteTypeID() const { return m_liteTypeId; }

protected:
    unsigned int m_liteTypeId = ID;
};

// Override the lite rtti type ID on subsequent classes of a class hierarchy.
template <class Base, class Derived, unsigned int ID>
class lite_rtti_override : public Base
{
public:
    constexpr static uint32_t LITE_RTTI_TYPE_ID = ID;
    lite_rtti_override() { Base::m_liteTypeId = ID; }

    template <typename... Args>
    lite_rtti_override(Args&&... args) : Base(std::forward<Args>(args)...)
    {
        Base::m_liteTypeId = ID;
    }
};

// Like dynamic_cast<>, but can only cast a pointer to its most-derived type.
template <class U, class T> U lite_rtti_cast(T* t)
{
    if (t != nullptr &&
        t->liteTypeID() == std::remove_pointer<U>::type::LITE_RTTI_TYPE_ID)
    {
        return static_cast<U>(t);
    }
    return nullptr;
}

template <class U, class T> rcp<U> lite_rtti_rcp_cast(rcp<T> t)
{
    if (t != nullptr &&
        t->liteTypeID() == std::remove_pointer<U>::type::LITE_RTTI_TYPE_ID)
    {
        return static_rcp_cast<U>(t);
    }
    return nullptr;
}

// Different versions of clang-format disagree on how to formate these.
// clang-format off

#define ENABLE_LITE_RTTI(ROOT) enable_lite_rtti<ROOT, CONST_ID(ROOT)>
#define LITE_RTTI_OVERRIDE(BASE, DERRIVED) lite_rtti_override<BASE, DERRIVED, CONST_ID(DERRIVED)>

#define LITE_RTTI_CAST_OR_RETURN(NAME, TYPE, POINTER)                                              \
    auto NAME = rive::lite_rtti_cast<TYPE>(POINTER);                                                     \
    if (NAME == nullptr)                                                                           \
        return

#define LITE_RTTI_CAST_OR_BREAK(NAME, TYPE, POINTER)                                               \
    auto NAME = rive::lite_rtti_cast<TYPE>(POINTER);                                                     \
    if (NAME == nullptr)                                                                           \
        break

#define LITE_RTTI_CAST_OR_CONTINUE(NAME, TYPE, POINTER)                                            \
    auto NAME = rive::lite_rtti_cast<TYPE>(POINTER);                                                     \
    if (NAME == nullptr)                                                                           \
        continue
// clang-format on
} // namespace rive
