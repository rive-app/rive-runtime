/*
 * Copyright 2021 Rive
 */

#ifndef _RIVE_REFCNT_HPP_
#define _RIVE_REFCNT_HPP_

#include "rive/rive_types.hpp"

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

/*
 *  RefCnt : Threadsafe shared pointer baseclass.
 *
 *  The reference count is set to one in the constructor, and goes up on every call to ref(),
 *  and down on every call to unref(). When a call to unref() brings the counter to 0,
 *  delete is called on the object.
 *
 *  rcp : template wrapper for subclasses of RefCnt, to manage assignment and parameter passing
 *  to safely keep track of shared ownership.
 *
 *  Both of these inspired by Skia's SkRefCnt and sk_sp
 */

namespace rive {

    class RefCnt {
    public:
        RefCnt() : m_refcnt(1) {}

        virtual ~RefCnt() { assert(this->debugging_refcnt() == 1); }

        void ref() const { (void)m_refcnt.fetch_add(+1, std::memory_order_relaxed); }

        void unref() const {
            if (1 == m_refcnt.fetch_add(-1, std::memory_order_acq_rel)) {
#ifndef NDEBUG
                // we restore the "1" in debug builds just to make our destructor happy
                (void)m_refcnt.fetch_add(+1, std::memory_order_relaxed);
#endif
                delete this;
            }
        }

        // not reliable in actual threaded scenarios, but useful (perhaps) for debugging
        int32_t debugging_refcnt() const { return m_refcnt.load(std::memory_order_relaxed); }

    private:
        // mutable, so can be changed even on a const object
        mutable std::atomic<int32_t> m_refcnt;

        RefCnt(RefCnt&&) = delete;
        RefCnt(const RefCnt&) = delete;
        RefCnt& operator=(RefCnt&&) = delete;
        RefCnt& operator=(const RefCnt&) = delete;
    };

    template <typename T> static inline T* safe_ref(T* obj) {
        if (obj) {
            obj->ref();
        }
        return obj;
    }

    template <typename T> static inline void safe_unref(T* obj) {
        if (obj) {
            obj->unref();
        }
    }

    // rcp : smart point template for holding subclasses of RefCnt

    template <typename T> class rcp {
    public:
        constexpr rcp() : m_ptr(nullptr) {}
        constexpr rcp(std::nullptr_t) : m_ptr(nullptr) {}
        explicit rcp(T* ptr) : m_ptr(ptr) {}

        rcp(const rcp<T>& other) : m_ptr(safe_ref(other.get())) {}
        rcp(rcp<T>&& other) : m_ptr(other.release()) {}

        /**
         *  Calls unref() on the underlying object pointer.
         */
        ~rcp() { safe_unref(m_ptr); }

        rcp<T>& operator=(std::nullptr_t) {
            this->reset();
            return *this;
        }

        rcp<T>& operator=(const rcp<T>& other) {
            if (this != &other) {
                this->reset(safe_ref(other.get()));
            }
            return *this;
        }

        rcp<T>& operator=(rcp<T>&& other) {
            this->reset(other.release());
            return *this;
        }

        T& operator*() const {
            assert(this->get() != nullptr);
            return *this->get();
        }

        explicit operator bool() const { return this->get() != nullptr; }

        T* get() const { return m_ptr; }
        T* operator->() const { return m_ptr; }

        // Unrefs the current pointer, and accepts the new pointer, but
        // DOES NOT increment ownership of the new pointer.
        void reset(T* ptr = nullptr) {
            // Calling m_ptr->unref() may call this->~() or this->reset(T*).
            // http://wg21.cmeerw.net/lwg/issue998
            // http://wg21.cmeerw.net/lwg/issue2262
            T* oldPtr = m_ptr;
            m_ptr = ptr;
            safe_unref(oldPtr);
        }

        // This returns the bare point WITHOUT CHANGING ITS REFCNT, but removes it
        // from this object, so the caller must manually manage its count.
        T* release() {
            T* ptr = m_ptr;
            m_ptr = nullptr;
            return ptr;
        }

        void swap(rcp<T>& other) { std::swap(m_ptr, other.m_ptr); }

    private:
        T* m_ptr;
    };

    template <typename T> inline void swap(rcp<T>& a, rcp<T>& b) { a.swap(b); }

    // == variants

    template <typename T> inline bool operator==(const rcp<T>& a, std::nullptr_t) { return !a; }
    template <typename T> inline bool operator==(std::nullptr_t, const rcp<T>& b) { return !b; }
    template <typename T, typename U> inline bool operator==(const rcp<T>& a, const rcp<U>& b) {
        return a.get() == b.get();
    }

    // != variants

    template <typename T> inline bool operator!=(const rcp<T>& a, std::nullptr_t) {
        return static_cast<bool>(a);
    }
    template <typename T> inline bool operator!=(std::nullptr_t, const rcp<T>& b) {
        return static_cast<bool>(b);
    }
    template <typename T, typename U> inline bool operator!=(const rcp<T>& a, const rcp<U>& b) {
        return a.get() != b.get();
    }

} // namespace rive

#endif
