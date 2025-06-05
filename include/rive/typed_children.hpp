#ifndef _RIVE_TYPED_CHILDREN_HPP_
#define _RIVE_TYPED_CHILDREN_HPP_

#include "rive/core.hpp"
#include "rive/span.hpp"

namespace rive
{
class Core;
template <typename T> class TypedChild
{
public:
    TypedChild(Core** child, Core** end) : m_child(child), m_end(end) {}

    T* operator*() const
    {
        auto child = *m_child;
        if (child == nullptr)
        {
            return nullptr;
        }
        return child->template as<T>();
    }

    TypedChild& operator++()
    {
        m_child++;
        while (m_child != m_end && (*m_child) != nullptr &&
               !(*m_child)->template is<T>())
        {
            m_child++;
        }
        return *this;
    }

    bool operator==(const TypedChild& o) const { return m_child == o.m_child; }

    bool operator!=(const TypedChild& o) const { return m_child != o.m_child; }

private:
    Core** m_child;
    Core** m_end;
};

template <typename T> class TypedChildren
{
public:
    TypedChildren(Span<Core*> children) : m_children(children) {}

    TypedChild<T> begin() const
    {
        size_t size = m_children.size();
        size_t index = 0;
        while (index < size && !m_children[index]->template is<T>())
        {
            index++;
        }
        return TypedChild<T>(m_children.data() + index,
                             m_children.data() + size);
    }

    TypedChild<T> end() const
    {
        auto ptr = m_children.data() + m_children.size();
        return TypedChild<T>(ptr, ptr);
    }

    T* first() const
    {
        auto start = begin();
        if (start == end())
        {
            return nullptr;
        }
        return *start;
    }

    size_t size() const
    {
        size_t count = 0;
        auto last = end();
        for (auto itr = begin(); itr != last; ++itr)
        {
            count++;
        }
        return count;
    }

private:
    Span<Core*> m_children;
};
} // namespace rive
#endif