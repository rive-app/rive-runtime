#include "rive/solo.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/focus_data.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "rive/artboard.hpp"
#include "rive/node.hpp"
#include "rive/container_component.hpp"
#include "rive/transform_component.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_node_provider.hpp"

using namespace rive;

// Some child components shouldn't be considered as part of the solo set as they
// are more akin to properties/metadata of the solo itself (constraints,
// clipping shapes, focus and semantic data) rather than selectable solo
// options. These are excluded both from collapse propagation and from
// index/name based selection so that data binding targets only the real solo
// options.
static bool isSoloSetMember(Component* child)
{
    return !(child->is<Constraint>() || child->is<ClippingShape>() ||
             child->is<FocusData>() || child->is<SemanticData>());
}

Component* Solo::activeComponent()
{
    auto* ab = artboard();
    Core* active = ab != nullptr ? ab->resolve(activeComponentId()) : nullptr;
    // The active id refers to one of our children; match by pointer so we get
    // it typed as a Component.
    for (Component* child : children())
    {
        if (child == active)
        {
            return child;
        }
    }
    return nullptr;
}

#ifdef WITH_RIVE_LAYOUT
void Solo::recollectOwningLayout()
{
    // We are not a LayoutComponent, so walk up for the layout that owns the
    // slot our active child occupies.
    for (Component* p = parent(); p != nullptr; p = p->parent())
    {
        if (p->is<LayoutComponent>())
        {
            p->as<LayoutComponent>()->syncLayoutChildren();
            return;
        }
    }
}
#endif

void Solo::propagateCollapse(bool collapse)
{
    Core* active =
        collapse ? nullptr : artboard()->resolve(activeComponentId());
    for (Component* child : children())
    {
        // For components that aren't part of the solo set, simply pass on the
        // collapse value of the solo itself.
        if (!isSoloSetMember(child))
        {
            child->collapse(collapse);
            continue;
        }

        // This child is part of the solo set so only make it active if it's the
        // currently marked solo object.
        child->collapse(child != active);
    }
}

bool Solo::collapse(bool value)
{
    // Intentionally using Component instead of Super as we don't want to call
    // collapse on the Container logic which just propagates blindly to
    // children.
    if (!Component::collapse(value))
    {
        return false;
    }
    propagateCollapse(value);
    return true;
}

void Solo::activeComponentIdChanged()
{
    propagateCollapse(isCollapsed());
#ifdef WITH_RIVE_LAYOUT
    // Only the active child is exposed to layout, so a swap changes the owning
    // layout's child set.
    recollectOwningLayout();
#endif
}

StatusCode Solo::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    propagateCollapse(isCollapsed());
#ifdef WITH_RIVE_LAYOUT
    // Parent chain + active child are resolved now; make sure the owning layout
    // has collected the active child.
    recollectOwningLayout();
#endif
    return StatusCode::Ok;
}

void Solo::updateByIndex(size_t index)
{
    // The number of solo options is always <= children().size(), so any index
    // that big can never match. Bail early to avoid an O(n) walk every frame
    // for out-of-range indices (the data-binding path casts rounded floats to
    // size_t without clamping, so negative values arrive as huge indices).
    if (!artboard() || index >= children().size())
    {
        return;
    }
    // The index refers to the Nth solo option, skipping property-like children
    // (constraints, clipping shapes, focus/semantic data) so it matches the
    // ordering exposed by getActiveChildIndex.
    size_t soloIndex = 0;
    for (auto& child : children())
    {
        if (!isSoloSetMember(child))
        {
            continue;
        }
        if (soloIndex == index)
        {
            activeComponentId(artboard()->idOf(child));
            return;
        }
        soloIndex++;
    }
}

void Solo::updateByName(const std::string& name)
{
    if (!artboard())
    {
        return;
    }
    for (auto& child : children())
    {
        if (!isSoloSetMember(child))
        {
            continue;
        }
        if (child->name() == name)
        {

            int globalIndex = artboard()->idOf(child);
            activeComponentId(globalIndex);
            break;
        }
    }
}

int Solo::getActiveChildIndex()
{
    if (!artboard())
    {
        return -1;
    }
    Core* active = artboard()->resolve(activeComponentId());
    if (active)
    {
        int index = 0;
        for (auto& child : children())
        {
            if (!isSoloSetMember(child))
            {
                continue;
            }
            if (child == active)
            {
                return index;
            }
            index++;
        }
    }
    return -1;
}

std::string Solo::getActiveChildName()
{
    if (!artboard())
    {
        return "";
    }
    Core* active = artboard()->resolve(activeComponentId());
    if (active && active->is<Component>())
    {
        return active->as<Component>()->name();
    }
    return "";
}
