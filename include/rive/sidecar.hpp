#ifndef _RIVE_SIDECAR_HPP_
#define _RIVE_SIDECAR_HPP_

#include <atomic>
#include <cstdint>
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
  Sidecar(const Sidecar& other)
      : m_v(other.m_v != nullptr ? new T(*other.m_v) : nullptr)
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
  T* ensureAllocated()
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

// A Sidecar for the generated authored property clusters (a component's name,
// a layout style's cold fields), which a copy shares with the object it was
// copied from until either side writes to it. Copying takes a reference
// instead of allocating, and the first write to a shared cluster gives the
// writer a copy of its own. An artboard instance clones every component of
// its source, and almost none of them write these again, so most instances
// never allocate their own.
//
// Writes go through ensureAllocated(); get() is read-only, so nothing can
// write into a cluster another object still shares. The count is atomic as
// instances of one source can be made on different threads.
template <typename T> class SharedSidecar
{
public:
  SharedSidecar() = default;
  ~SharedSidecar() { release(); }

  SharedSidecar(const SharedSidecar& other) : m_b(other.m_b) { retain(); }
  SharedSidecar& operator=(const SharedSidecar& other)
  {
    if (m_b != other.m_b)
    {
      other.retain();
      release();
      m_b = other.m_b;
    }
    return *this;
  }

  SharedSidecar(SharedSidecar&& other) noexcept : m_b(other.m_b)
  {
    other.m_b = nullptr;
  }
  SharedSidecar& operator=(SharedSidecar&& other) noexcept
  {
    if (this != &other)
    {
      release();
      m_b = other.m_b;
      other.m_b = nullptr;
    }
    return *this;
  }

  const T* get() const { return m_b != nullptr ? &m_b->value : nullptr; }

  // Allocate on first use, or copy a shared cluster, then return the backing
  // object, ours alone. Generated setters and deserialize write through it.
  T* ensureAllocated()
  {
    if (m_b == nullptr)
    {
      m_b = new Block();
    }
    else if (m_b->refs.load(std::memory_order_acquire) != 1)
    {
      Block* own = new Block(m_b->value);
      release();
      m_b = own;
    }
    return &m_b->value;
  }

  void reset()
  {
    release();
    m_b = nullptr;
  }

#ifdef TESTING
  // Whether another object holds this cluster too.
  bool isShared() const
  {
    return m_b != nullptr && m_b->refs.load(std::memory_order_acquire) != 1;
  }
#endif

private:
  struct Block
  {
    Block() = default;
    explicit Block(const T& from) : value(from) {}
    T value;
    std::atomic<uint32_t> refs{1};
  };

  void retain() const
  {
    if (m_b != nullptr)
    {
      m_b->refs.fetch_add(1, std::memory_order_relaxed);
    }
  }
  void release()
  {
    if (m_b != nullptr &&
        m_b->refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
      delete m_b;
    }
  }

  Block* m_b = nullptr;
};

} // namespace rive
#endif
