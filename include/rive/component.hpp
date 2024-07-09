#ifndef _RIVE_COMPONENT_HPP_
#define _RIVE_COMPONENT_HPP_
#include "rive/component_dirt.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/dependency_helper.hpp"

#include <vector>
#include <functional>

namespace rive
{
class ContainerComponent;
class Artboard;

class Component : public ComponentBase
{
    friend class Artboard;

private:
    ContainerComponent* m_Parent = nullptr;

    unsigned int m_GraphOrder;
    Artboard* m_Artboard = nullptr;

protected:
    ComponentDirt m_Dirt = ComponentDirt::Filthy;

public:
    DependencyHelper<Artboard, Component> m_DependencyHelper;
    virtual bool collapse(bool value);
    inline Artboard* artboard() const { return m_Artboard; }
    StatusCode onAddedDirty(CoreContext* context) override;
    inline ContainerComponent* parent() const { return m_Parent; }
    const std::vector<Component*>& dependents() const { return m_DependencyHelper.dependents(); }

    void addDependent(Component* component);

    // TODO: re-evaluate when more of the lib is complete...
    // These could be pure virtual but we define them empty here to avoid
    // having to implement them in a bunch of concrete classes that
    // currently don't use this logic.
    virtual void buildDependencies() {}
    virtual void onDirty(ComponentDirt dirt) {}
    virtual void update(ComponentDirt value) {}

    unsigned int graphOrder() const { return m_GraphOrder; }
    bool addDirt(ComponentDirt value, bool recurse = false);
    inline bool hasDirt(ComponentDirt flag) const { return (m_Dirt & flag) == flag; }
    static inline bool hasDirt(ComponentDirt value, ComponentDirt flag)
    {
        return (value & flag) != ComponentDirt::None;
    }

    StatusCode import(ImportStack& importStack) override;

    bool isCollapsed() const
    {
        return (m_Dirt & ComponentDirt::Collapsed) == ComponentDirt::Collapsed;
    }
};
} // namespace rive

#endif
