#ifndef _RIVE_LAYOUT_NODE_PROVIDER_HPP_
#define _RIVE_LAYOUT_NODE_PROVIDER_HPP_

#include "rive/layout/layout_enums.hpp"
#include "rive/math/aabb.hpp"
#include <assert.h>
#include <vector>

namespace rive
{
class Component;
class KeyFrameInterpolator;
class LayoutConstraint;
class TransformComponent;

class LayoutStyleApplier;

class LayoutNodeProvider
{
protected:
    // Lazily allocated: null (8 bytes, no heap) until this provider actually
    // gains a layout constraint — which most instances never do.
    std::vector<LayoutConstraint*>* m_layoutConstraints = nullptr;

    // Read access; returns a shared empty list when none are allocated.
    const std::vector<LayoutConstraint*>& layoutConstraints() const
    {
        static const std::vector<LayoutConstraint*> empty;
        return m_layoutConstraints != nullptr ? *m_layoutConstraints : empty;
    }

public:
    virtual ~LayoutNodeProvider() { delete m_layoutConstraints; }
#ifdef WITH_RIVE_LAYOUT
    virtual void* layoutNode(int index) = 0;
#endif
    static LayoutNodeProvider* from(Component* component);
    virtual TransformComponent* transformComponent() = 0;
    void addLayoutConstraint(LayoutConstraint* constraint);
    virtual AABB layoutBounds() = 0;
    virtual AABB layoutBoundsForNode(int index) { return layoutBounds(); }
    virtual bool syncStyleChanges() { return false; };
    virtual void updateLayoutBounds(bool animate = true) {};
    virtual void markLayoutNodeDirty(bool shouldForceUpdateLayoutBounds = false)
    {}
#ifdef WITH_RIVE_LAYOUT
    /// Registers an object that contributes to this provider's yoga style.
    /// Lets an applier attach without caring whether its owner is a
    /// LayoutComponent or a LayoutParticipant, and without the provider having
    /// to publish its LayoutData. Runtime objects are never removed, so there
    /// is no unregister — the editor handles that in Dart.
    virtual void addLayoutStyleApplier(LayoutStyleApplier* applier) {}
#endif
    virtual size_t numLayoutNodes() = 0;
#ifdef WITH_RIVE_LAYOUT
    virtual bool cascadeLayoutStyle(
        LayoutStyleInterpolation inheritedInterpolation,
        KeyFrameInterpolator* inheritedInterpolator,
        float inheritedInterpolationTime,
        LayoutDirection direction)
    {
        return false;
    }
#endif
};
} // namespace rive

#endif