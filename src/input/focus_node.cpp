/*
 * Copyright 2024 Rive
 */

#include "rive/input/focus_node.hpp"
#include <algorithm>

namespace rive
{

void FocusNode::addChild(rcp<FocusNode> child)
{
    if (!child)
    {
        return;
    }

    // Remove from old parent if exists
    child->removeFromParent();

    child->m_parent = this;
    m_children.push_back(std::move(child));
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
}

void FocusNode::removeFromParent()
{
    if (m_parent)
    {
        m_parent->removeChild(ref_rcp(this));
    }
}

} // namespace rive
