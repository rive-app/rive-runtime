#ifndef _RIVE_DEPENDENCY_HELPER_HPP_
#define _RIVE_DEPENDENCY_HELPER_HPP_

#include "rive/component_dirt.hpp"

namespace rive
{
class Component;
// class DependencyRoot
// {
// public:
//     virtual void onComponentDirty(Component* component) {};
// };

template <typename T, typename U> class DependencyHelper
{
    std::vector<U*> m_Dependents;
    T* m_dependecyRoot;

public:
    void dependecyRoot(T* value) { m_dependecyRoot = value; }
    DependencyHelper(T* value) : m_dependecyRoot(value) {}
    DependencyHelper() {}
    void addDependent(U* component)
    {
        // Make it's not already a dependent.
        if (std::find(m_Dependents.begin(), m_Dependents.end(), component) != m_Dependents.end())
        {
            return;
        }
        m_Dependents.push_back(component);
    }
    void addDirt(ComponentDirt value)
    {
        for (auto d : m_Dependents)
        {
            d->addDirt(value, true);
        }
    }

    void onComponentDirty(U* component) { m_dependecyRoot->onComponentDirty(component); }

    const std::vector<U*>& dependents() const { return m_Dependents; }
};
} // namespace rive
#endif
