#include "rive/component.hpp"
#include "rive/artboard.hpp"
#include "rive/container_component.hpp"
#include "rive/core_context.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include <algorithm>

using namespace rive;

StatusCode Component::onAddedDirty(CoreContext* context)
{
    m_Artboard = static_cast<Artboard*>(context);
    m_DependencyHelper.dependecyRoot(m_Artboard);
    if (this == m_Artboard)
    {
        // We're the artboard, don't parent to ourselves.
        return StatusCode::Ok;
    }
    auto coreObject = context->resolve(parentId());
    if (coreObject == nullptr || !coreObject->is<ContainerComponent>())
    {
        return StatusCode::MissingObject;
    }
    m_Parent = static_cast<ContainerComponent*>(coreObject);
    m_Parent->addChild(this);
    return StatusCode::Ok;
}

void Component::addDependent(Component* component) { m_DependencyHelper.addDependent(component); }

bool Component::addDirt(ComponentDirt value, bool recurse)
{
    if ((m_Dirt & value) == value)
    {
        // Already marked.
        return false;
    }

    // Make sure dirt is set before calling anything that can set more dirt.
    m_Dirt |= value;

    onDirty(m_Dirt);

    m_DependencyHelper.onComponentDirty(this);

    if (!recurse)
    {
        return true;
    }

    m_DependencyHelper.addDirt(value);
    return true;
}

StatusCode Component::import(ImportStack& importStack)
{
    if (is<Artboard>())
    {
        // Artboards are always their first object.
        assert(as<Artboard>()->objects().size() == 0);
        as<Artboard>()->addObject(this);
        return Super::import(importStack);
    }

    auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addComponent(this);
    return Super::import(importStack);
}

bool Component::collapse(bool value)
{
    if (isCollapsed() == value)
    {
        return false;
    }
    if (value)
    {
        m_Dirt |= ComponentDirt::Collapsed;
    }
    else
    {
        m_Dirt &= ~ComponentDirt::Collapsed;
    }
    onDirty(m_Dirt);
    m_DependencyHelper.onComponentDirty(this);
    return true;
}