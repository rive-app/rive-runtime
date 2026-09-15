/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SPAN_HPP_
#define _RIVE_SPAN_HPP_

#include "rive/rive_types.hpp"

#include <initializer_list>
#include <type_traits>
#include <iterator>

/*
 *  Span : cheap impl of std::span (which is C++20)
 *
 *  Inspired by Skia's SkSpan
 */

namespace rive
{
namespace internal
{

template <typename T, typename = void> struct IsSpannableImpl : std::false_type
{};

// A type is "spannable" if it has a data() member that returns a pointer and a
// size() member that returns an integer. This is true for Span, std::span,
// std::vector, etc.
template <typename T>
struct IsSpannableImpl<
    T,
    std::enable_if_t<std::is_pointer_v<decltype(std::declval<T>().data())> &&
                         std::is_integral_v<decltype(std::declval<T>().size())>,
                     void>> : std::true_type
{};

template <typename T> constexpr bool IsSpannable = IsSpannableImpl<T>::value;

template <typename SpannableType, typename ElementType, typename = void>
struct IsTypedSpannableImpl : std::false_type
{};

template <typename SpannableType, typename ElementType>
struct IsTypedSpannableImpl<
    SpannableType,
    ElementType,
    std::enable_if_t<
        std::is_assignable_v<ElementType*&,
                             decltype(std::declval<SpannableType>().data())>,
        void>> : IsSpannableImpl<SpannableType>
{};

template <typename S, typename E>
constexpr bool IsTypedSpannable = IsTypedSpannableImpl<S, E>::value;

template <typename S, typename = std::enable_if_t<IsSpannable<S>, void>>
using SpannableElementType =
    std::remove_pointer_t<decltype(std::declval<S>().data())>;

} // namespace internal

template <typename T> class Span
{
    T* m_Ptr;
    size_t m_Size;

public:
    constexpr Span() : m_Ptr(nullptr), m_Size(0) {}
    constexpr Span(T* ptr, size_t size) : m_Ptr(ptr), m_Size(size)
    {
        assert(ptr <= ptr + size);
    }

    template <size_t N> constexpr Span(T (&array)[N]) : m_Ptr(array), m_Size(N)
    {}

    // Handle assignment from any types that have data and size members
    template <
        typename U,
        typename =
            typename std::enable_if_t<internal::IsTypedSpannable<U, T>, U>>
    constexpr Span(U& that) : Span(that.data(), that.size())
    {}

    template <typename U,
              typename = typename std::
                  enable_if_t<internal::IsTypedSpannable<const U, T>, U>>
    constexpr Span(const U& that) : Span(that.data(), that.size())
    {}

    T& operator[](size_t index) const
    {
        assert(index < m_Size);
        return m_Ptr[index];
    }

    constexpr T* data() const { return m_Ptr; }
    constexpr size_t size() const { return m_Size; }
    constexpr bool empty() const { return m_Size == 0; }

    constexpr T* begin() const { return m_Ptr; }
    constexpr T* end() const { return m_Ptr + m_Size; }

    constexpr T& front() const { return (*this)[0]; }
    constexpr T& back() const { return (*this)[m_Size - 1]; }

    // returns byte-size of the entire span
    constexpr size_t size_bytes() const { return m_Size * sizeof(T); }

    constexpr size_t count() const { return m_Size; }

    Span<T> subset(size_t offset, size_t size) const
    {
        assert(offset <= m_Size);
        assert(size <= m_Size - offset);
        return {m_Ptr + offset, size};
    }

    // Makes rive::Span std::Container compatible
    // https://en.cppreference.com/w/cpp/named_req/Container
    typedef typename std::remove_cv<T>::type value_type;
    typedef T& reference;
    typedef T const& const_reference;
    typedef T* iterator;
    typedef T const* const_iterator;
    typedef std::ptrdiff_t difference_type;
    typedef size_t size_type;

    bool operator!=(const Span<T>& that) const
    {
        return m_Ptr != that.m_Ptr || m_Size != that.m_Size;
    }
    bool operator==(const Span<T>& that) const
    {
        return m_Ptr == that.m_Ptr && m_Size == that.m_Size;
    }
};

// Deduction guides so that spannable types will deduce the template type
// properly.
template <typename T>
Span(const T&) -> Span<internal::SpannableElementType<const T>>;

template <typename T> Span(T&) -> Span<internal::SpannableElementType<const T>>;

// Deduction guide to make array conversions automatic
template <typename T, size_t N> Span(T (&)[N]) -> Span<T>;

template <typename T> Span<T> make_span(T* ptr, size_t size)
{
    return Span<T>(ptr, size);
}

template <typename T, size_t N> Span<T> make_span(T (&ptr)[N])
{
    return Span<T>(ptr, N);
}
} // namespace rive

#endif
