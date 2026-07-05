#ifndef _RIVE_LAZY_VECTOR_HPP_
#define _RIVE_LAZY_VECTOR_HPP_

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace rive
{

// 8 B owning pointer to a heap-allocated std::vector<T>, allocated on first
// mutation. Saves 16 B per object compared to an inline std::vector<T> (24 B
// header) for fields that are empty most of the time.
//
// Use when you have a per-object list that's empty for the majority of
// instances (typical fits: dependents/observers/optional adornments). Do NOT
// use when the vector is usually populated — the extra heap allocation makes
// it strictly worse than a regular vector for that case.
//
// All read operations are safe on an empty (never-allocated) wrapper. The
// hot-path null check is a single load + branch; reads return iterators into
// a shared static empty vector when the wrapper has no backing storage.
template <typename T> class LazyVector
{
public:
    using value_type = T;
    using const_iterator = typename std::vector<T>::const_iterator;

    LazyVector() = default;
    ~LazyVector() { delete m_v; }

    // Deep copy. Preserves the "copying the owner clones its list" semantic
    // that a plain std::vector member would have provided. An empty source
    // (m_v == nullptr) copies as an empty wrapper — no allocation. This
    // keeps Component / Drawable / etc. copyable (Core::clone path).
    LazyVector(const LazyVector& other) :
        m_v(other.m_v != nullptr ? new std::vector<T>(*other.m_v) : nullptr)
    {}
    LazyVector& operator=(const LazyVector& other)
    {
        if (this != &other)
        {
            delete m_v;
            m_v =
                other.m_v != nullptr ? new std::vector<T>(*other.m_v) : nullptr;
        }
        return *this;
    }

    LazyVector(LazyVector&& other) noexcept : m_v(other.m_v)
    {
        other.m_v = nullptr;
    }
    LazyVector& operator=(LazyVector&& other) noexcept
    {
        if (this != &other)
        {
            delete m_v;
            m_v = other.m_v;
            other.m_v = nullptr;
        }
        return *this;
    }

    bool empty() const { return m_v == nullptr || m_v->empty(); }
    std::size_t size() const { return m_v == nullptr ? 0 : m_v->size(); }

    // Append unconditionally. Allows duplicates (use pushUnique to dedup).
    void push_back(T value)
    {
        if (m_v == nullptr)
        {
            m_v = new std::vector<T>();
        }
        m_v->push_back(std::move(value));
    }

    // Append only if `value` is not already present. Centralizes the
    // find-then-push idiom used by callers that maintain set semantics.
    void pushUnique(T value)
    {
        if (m_v != nullptr &&
            std::find(m_v->begin(), m_v->end(), value) != m_v->end())
        {
            return;
        }
        push_back(std::move(value));
    }

    // Erase every element equal to `value`. No-op when wrapper is empty.
    // Uses the erase-remove idiom — safe but removes ALL duplicates if any.
    void eraseAll(const T& value)
    {
        if (m_v == nullptr)
        {
            return;
        }
        m_v->erase(std::remove(m_v->begin(), m_v->end(), value), m_v->end());
    }

    void clear()
    {
        if (m_v != nullptr)
        {
            m_v->clear();
        }
    }

    // Const iteration always works, even on a never-allocated wrapper —
    // returns iterators into a shared static empty vector when m_v is null.
    // Inlined null check (no view() indirection) so the compiler can keep
    // m_v in a register across both begin/end loads in a range-for.
    const_iterator begin() const
    {
        return m_v ? m_v->begin() : emptyView().begin();
    }
    const_iterator end() const { return m_v ? m_v->end() : emptyView().end(); }

    // Returns a stable const reference suitable as the return value of
    // accessor methods. Always valid — falls back to a shared empty when
    // the wrapper has no backing storage.
    const std::vector<T>& view() const { return m_v ? *m_v : emptyView(); }

private:
    std::vector<T>* m_v = nullptr;

    static const std::vector<T>& emptyView()
    {
        static const std::vector<T> EMPTY;
        return EMPTY;
    }
};

} // namespace rive
#endif
