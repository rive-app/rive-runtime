#include "rive/node.hpp"
#include "rive/world_transform_component.hpp"
#include "rive/layout_component.hpp"

using namespace rive;

void Node::xChanged() { markTransformDirty(); }
void Node::yChanged() { markTransformDirty(); }

Mat2D Node::localTransform()
{
    if (m_LocalTransformNeedsRecompute)
    {
        m_LocalTransformNeedsRecompute = false;
        if (m_ParentTransformComponent != nullptr)
        {
            auto parentWorld = m_ParentTransformComponent->worldTransform();
            Mat2D inverse;

            if (parentWorld.invert(&inverse))
            {
                m_LocalTransform = inverse * worldTransform();
                return m_LocalTransform;
            }
        }
        m_LocalTransform = Mat2D();
    }
    return m_LocalTransform;
}

#ifdef WITH_RIVE_LAYOUT
void Node::markLayoutNodeDirty()
{
    for (ContainerComponent* p = parent(); p != nullptr; p = p->parent())
    {
        if (p->is<LayoutComponent>())
        {
            p->as<LayoutComponent>()->markLayoutNodeDirty();
        }
    }
}
#endif