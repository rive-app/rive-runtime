#include "rive/component.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/nested_artboard_layout.hpp"

using namespace rive;

LayoutNodeProvider* LayoutNodeProvider::from(Component* component)
{
    switch (component->coreType())
    {
        case LayoutComponent::typeKey:
            return component->as<LayoutComponent>();
        case NestedArtboardLayout::typeKey:
            return component->as<NestedArtboardLayout>();
    }
    return nullptr;
}

void LayoutNodeProvider::addLayoutConstraint(LayoutConstraint* constraint)
{
    assert(std::find(m_layoutConstraints.begin(),
                     m_layoutConstraints.end(),
                     constraint) == m_layoutConstraints.end());
    m_layoutConstraints.push_back(constraint);
}