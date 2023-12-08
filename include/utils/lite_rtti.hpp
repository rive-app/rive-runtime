/*
 * Copyright 2023 Rive
 */

// "lite_rtti_cast<T*>()" is a very basic polyfill for "dynamic_cast<T*>()" that can only cast a
// pointer to its most-derived type. To use it, the base class must derive from enable_lite_rtti,
// and the subclass must inherit from lite_rtti_override:
//
//     class Root : public enable_lite_rtti<Root> {};
//     class Derived : public lite_rtti_override<Root, Derived> {};
//     Root* derived = new Derived();
//     lite_rtti_cast<Derived*>(derived);
//

#pragma once

#include "rive/refcnt.hpp"
#include <stdint.h>
#include <type_traits>

namespace rive
{
// Derive type IDs based on the unique address of a static placeholder value.
template <typename T>
typename std::enable_if<!std::is_const<T>::value, uintptr_t>::type lite_type_id()
{
    static int placeholderForUniqueAddress;
    return reinterpret_cast<uintptr_t>(&placeholderForUniqueAddress);
}

// Type IDs for const-qualified types should match their non-const counterparts.
template <typename T>
typename std::enable_if<std::is_const<T>::value, uintptr_t>::type lite_type_id()
{
    return lite_type_id<typename std::remove_const<T>::type>();
}

// Enable lite rtti on the root of a class hierarchy.
template <class Root> class enable_lite_rtti
{
public:
    uintptr_t liteTypeID() const { return m_liteTypeID; }

protected:
    uintptr_t m_liteTypeID = lite_type_id<Root>();
};

// Override the lite rtti type ID on subsequent classes of a class hierarchy.
template <class Base, class Derived> class lite_rtti_override : public Base
{
public:
    lite_rtti_override() { Base::m_liteTypeID = lite_type_id<Derived>(); }

    template <typename... Args>
    lite_rtti_override(Args&&... args) : Base(std::forward<Args>(args)...)
    {
        Base::m_liteTypeID = lite_type_id<Derived>();
    }
};

// Like dynamic_cast<>, but can only cast a pointer to its most-derived type.
template <class U, class T> U lite_rtti_cast(T* t)
{
    if (t != nullptr && t->liteTypeID() == lite_type_id<typename std::remove_pointer<U>::type>())
    {
        return static_cast<U>(t);
    }
    return nullptr;
}

template <class U, class T> rcp<U> lite_rtti_rcp_cast(rcp<T> t)
{
    if (t != nullptr && t->liteTypeID() == lite_type_id<U>())
    {
        return static_rcp_cast<U>(t);
    }
    return nullptr;
}

// Different versions of clang-format disagree on how to formate these.
// clang-format off
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
