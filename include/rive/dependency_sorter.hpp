#ifndef _RIVE_DEPENDENCYSORTER_HPP_
#define _RIVE_DEPENDENCYSORTER_HPP_

#include <unordered_set>
#include <vector>

namespace rive
{
class Component;
class DependencySorter
{
private:
    std::unordered_set<Component*> m_Perm;
    std::unordered_set<Component*> m_Temp;

public:
    void sort(Component* root, std::vector<Component*>& order);
    bool visit(Component* component, std::vector<Component*>& order);
};
} // namespace rive

#endif