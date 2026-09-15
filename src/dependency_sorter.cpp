#include "rive/dependency_sorter.hpp"
#include "rive/component.hpp"
#include <algorithm>

using namespace rive;

void DependencySorter::sort(Component* root, std::vector<Component*>& order)
{
    order.clear();
    visit(root, order);
    // visit() appends in finish order; the caller wants its reverse. See the
    // note in visit() on why this is not an insert at begin().
    std::reverse(order.begin(), order.end());
}

void DependencySorter::sort(std::vector<Component*> roots,
                            std::vector<Component*>& order)
{
    order.clear();
    for (auto root : roots)
    {
        visit(root, order);
    }
    std::reverse(order.begin(), order.end());
}

bool DependencySorter::visit(Component* component,
                             std::vector<Component*>& order)
{
    if (m_Perm.find(component) != m_Perm.end())
    {
        return true;
    }
    if (m_Temp.find(component) != m_Temp.end())
    {
        fprintf(stderr, "Dependency cycle!\n");
        return false;
    }

    m_Temp.emplace(component);

    // By reference: dependents() returns `const std::vector<U*>&`, and a plain
    // `auto` strips the reference, deep-copying the vector for every component
    // visited.
    const auto& dependents = component->dependents();
    for (auto dependent : dependents)
    {
        if (!visit(dependent, order))
        {
            return false;
        }
    }
    m_Perm.emplace(component);
    // Append and let sort() reverse once, rather than inserting at the front
    // per component: insert(begin()) shifts the whole vector every time, making
    // this O(n^2) in the component count. The Dart port already does it this
    // way -- see the same note in dependency_sorter.dart.
    order.push_back(component);

    return true;
}