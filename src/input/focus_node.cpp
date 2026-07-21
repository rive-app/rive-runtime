/*
 * Copyright 2024 Rive
 */

#include "rive/input/focus_node.hpp"
#include "rive/input/focus_manager.hpp"
#include <algorithm>
#include <cstddef>

namespace rive
{

FocusNode::~FocusNode()
{
    // A child held elsewhere via rcp (a NestedArtboard's persistent focus
    // scope, an ArtboardComponentList row) can outlive this node, so clear its
    // back-pointer to keep m_parent from dangling.
    for (const auto& child : m_children)
    {
        child->m_parent = nullptr;
    }
}

void FocusNode::addChild(rcp<FocusNode> child)
{
    insertChild(m_children.size(), std::move(child));
}

void FocusNode::insertChild(size_t index, rcp<FocusNode> child)
{
    if (!child)
    {
        return;
    }
    child->removeFromParent();
    child->m_parent = this;
    if (index > m_children.size())
    {
        index = m_children.size();
    }
    m_children.insert(m_children.begin() + static_cast<ptrdiff_t>(index),
                      std::move(child));
    invalidateFocusableContent();
}

void FocusNode::removeChild(rcp<FocusNode> child)
{
    if (!child || child->m_parent != this)
    {
        return;
    }

    child->m_parent = nullptr;
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end())
    {
        m_children.erase(it);
    }
    invalidateFocusableContent();
}

void FocusNode::invalidateFocusableContent()
{
    if (m_manager != nullptr)
    {
        m_manager->markFocusableContentDirty();
    }
}

void FocusNode::removeFromParent()
{
    if (m_parent)
    {
        m_parent->removeChild(ref_rcp(this));
    }
}

} // namespace rive
