#include "rive/component.hpp"
#include "rive/artboard.hpp"
#include "rive/container_component.hpp"
#include "rive/core_context.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/layout_component.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <algorithm>

using namespace rive;

bool Component::validate(CoreContext* context)
{
    auto coreObject = context->resolve(parentId());
    return coreObject != nullptr && coreObject->is<ContainerComponent>();
}

StatusCode Component::onAddedDirty(CoreContext* context)
{
    m_Artboard = static_cast<Artboard*>(context);
    if (this == m_Artboard)
    {
        // We're the artboard, don't parent to ourselves.
        return StatusCode::Ok;
    }
    m_Parent = context->resolve(parentId())->as<ContainerComponent>();
    m_Parent->addChild(this);
    return StatusCode::Ok;
}

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

    onComponentDirty(this);

    if (!recurse)
    {
        return true;
    }

    addDirtToDependents(value);
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

    auto artboardImporter =
        importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addComponent(this);
    return Super::import(importStack);
}

bool Component::collapse(bool value)
{
    if (((m_Dirt & ComponentDirt::Collapsed) == ComponentDirt::Collapsed) ==
        value)
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
    onComponentDirty(this);
    updateCollapsables();
    return true;
}

bool Component::hitTestPoint(const Vec2D& position,
                             bool skipOnUnclipped,
                             bool isPrimaryHit)
{
    if (parent())
    {
        return parent()->hitTestPoint(position, skipOnUnclipped, false);
    }
    return true;
}

void Component::addCollapsable(DataBind* collapsable)
{
    // pushUnique gives set semantics; the collapse side-effect should only
    // fire on first add, so detect that via size delta.
    auto sizeBefore = m_collapsables.size();
    m_collapsables.pushUnique(collapsable);
    if (m_collapsables.size() != sizeBefore)
    {
        collapsable->collapse(isCollapsed());
    }
}

void Component::updateCollapsables()
{
    auto collapsed = isCollapsed();
    for (auto* collapsable : m_collapsables)
    {
        collapsable->collapse(collapsed);
    }
}