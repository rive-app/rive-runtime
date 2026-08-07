#ifndef _RIVE_SIDECAR_HPP_
#define _RIVE_SIDECAR_HPP_

#include <utility>

namespace rive
{

// 8 B owning pointer to a heap-allocated T, allocated lazily on first authored
// write. Used by the generated Core base classes to hoist a cluster of rarely
// authored ("cold") properties out of the inline object: the base pays only
// this pointer (null) until the cluster is touched, instead of the full inline
// size of every field in the cluster.
//
// Use when a group of properties is default on the majority of instances and is
// never read on the per-frame hot path. Do NOT use for properties that are set
// or read on most objects — the extra heap allocation + the null check on every
// access make it strictly worse than an inline field for that case.
//
// All read paths are safe on a never-allocated wrapper (get() returns nullptr);
// the generated getters fall back to the property's compile-time default. The
// hot-path null check is a single load + branch.
//
// Copy/move mirror LazyVector so Core subclasses stay copyable (the Core::clone
// path is default-construct + copy(), and Core deliberately keeps value
// semantics). A bare std::unique_ptr member would instead delete the containing
// class's implicit copy constructor.
template <typename T> class Sidecar
{
public:
    Sidecar() = default;
    ~Sidecar() { delete m_v; }

    // Deep copy. Preserves the "copying the owner clones its cold state"
    // semantic an inline field would have provided. An empty source
    // (m_v == nullptr) copies as an empty wrapper — no allocation.
    Sidecar(const Sidecar& other) :
        m_v(other.m_v != nullptr ? new T(*other.m_v) : nullptr)
    {}
    Sidecar& operator=(const Sidecar& other)
    {
        if (this != &other)
        {
            delete m_v;
            m_v = other.m_v != nullptr ? new T(*other.m_v) : nullptr;
        }
        return *this;
    }

    Sidecar(Sidecar&& other) noexcept : m_v(other.m_v) { other.m_v = nullptr; }
    Sidecar& operator=(Sidecar&& other) noexcept
    {
        if (this != &other)
        {
            delete m_v;
            m_v = other.m_v;
            other.m_v = nullptr;
        }
        return *this;
    }

    // Backing object, or nullptr when never allocated. The const overload
    // returns a const T* so a const owner (e.g. a generated `const` getter)
    // can read the payload but not mutate it through the sidecar; non-const
    // owners get a writable T*.
    const T* get() const { return m_v; }
    T* get() { return m_v; }

    // Allocate on first use, then return the backing object. Generated setters
    // and deserialize call this before writing an authored value.
    T* ensure()
    {
        if (m_v == nullptr)
        {
            m_v = new T();
        }
        return m_v;
    }

    void reset()
    {
        delete m_v;
        m_v = nullptr;
    }

private:
    T* m_v = nullptr;
};

} // namespace rive
#endif
