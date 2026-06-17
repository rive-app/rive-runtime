#ifndef _RIVE_DEPENDENCY_HELPER_HPP_
#define _RIVE_DEPENDENCY_HELPER_HPP_

#include "rive/component_dirt.hpp"
#include "rive/lazy_vector.hpp"

namespace rive
{
class Component;

// CRTP base. Holds a list of dependents; defers the "root" pointer to the
// derived class so we don't store it twice. Derived must expose:
//   T* dependencyRoot() const;
//
// Saves 8 B per Component / per ViewModelInstanceValue compared to the
// previous design where DependencyHelper also stored its own T* root pointer
// duplicating whatever the owner already had (Component::m_Artboard,
// ViewModelInstanceValue::m_viewModelInstance).
template <typename T, typename U, typename Derived> class DependencyHelper
{
    // Most Components have no dependents (only the dependency graph's
    // interior nodes do); for those, a single null pointer beats a 24 B
    // vector header by 16 B per Component.
    //
    // Hot path: addDirtToDependents() is called once per dirt cascade,
    // potentially many times per frame. The LazyVector null check is a
    // single load + branch, comparable to (or cheaper than) loading the
    // 3-pointer vector header inline.
    LazyVector<U*> m_Dependents;

public:
    void addDependent(U* component) { m_Dependents.pushUnique(component); }
    void removeDependent(U* component) { m_Dependents.eraseAll(component); }

    // Cascade dirt to every registered dependent. Named distinctly from
    // Component::addDirt (and the like on the derived class) so the inherited
    // method doesn't clash with the derived class's self-dirty method.
    //
    // Hot path: cascaded every dirt, often many times per frame. The empty()
    // early-out is a single load + branch when the wrapper's pointer is null,
    // identical in cost to the raw-pointer version this replaced.
    void addDirtToDependents(ComponentDirt value)
    {
        if (m_Dependents.empty())
        {
            return;
        }
        for (auto* d : m_Dependents)
        {
            d->addDirt(value, true);
        }
    }

    void onComponentDirty(U* component)
    {
        static_cast<Derived*>(this)->dependencyRoot()->onComponentDirty(
            component);
    }

    const std::vector<U*>& dependents() const { return m_Dependents.view(); }
};
} // namespace rive
#endif
