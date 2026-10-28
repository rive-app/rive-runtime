/*
 * Copyright 2026 Rive
 */

#pragma once

namespace rive
{
// Iterates a singly linked list through its nodes' "next" pointers.
template <typename T> class NextPtrIterator
{
public:
  explicit NextPtrIterator(T* node) : m_node(node) {}

  T& operator*() const { return *m_node; }
  T* operator->() const { return m_node; }

  NextPtrIterator& operator++()
  {
    m_node = m_node->next;
    return *this;
  }

  bool operator==(const NextPtrIterator& other) const
  {
    return m_node == other.m_node;
  }
  bool operator!=(const NextPtrIterator& other) const
  {
    return m_node != other.m_node;
  }

private:
  T* m_node;
};

// The nodes from "first" up to, but not including, "end", for range-for.
template <typename T> class NextPtrRange
{
public:
  explicit NextPtrRange(T* first, T* end = nullptr) : m_first(first), m_end(end)
  {}

  NextPtrIterator<T> begin() const { return NextPtrIterator<T>(m_first); }
  NextPtrIterator<T> end() const { return NextPtrIterator<T>(m_end); }

private:
  T* m_first;
  T* m_end;
};
} // namespace rive
