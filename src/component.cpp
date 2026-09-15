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

#ifdef WITH_RIVE_EDITOR
Component::OnParentIdChangedCallback Component::s_onParentIdChanged = nullptr;

void Component::parentIdChanged()
{
    if (s_onParentIdChanged != nullptr)
    {
        s_onParentIdChanged(this);
    }
}
#endif

bool Component::validate(CoreContext* context)
{
    auto coreObject = context->resolve(parentId());
    return coreObject != nullptr && coreObject->is<ContainerComponent>();
}

StatusCode Component::onAddedDirty(CoreContext* context)
{
#ifdef WITH_RIVE_EDITOR
    // editor_native's coop-apply passes an EditorFile as the
    // CoreContext — NOT an Artboard. The runtime's `static_cast` below
    // is invalid in that case (produces garbage). Mirror Dart's
    // `resolveArtboard()` in `packages/rive_core/lib/component.dart:175`:
    // walk up the parent chain by id to find the Artboard. Uses
    // `parentId()` (populated in Pass 2 of the coop-apply ordering)
    // rather than `parent()` pointers, so result is independent of
    // whether the parent's own `onAddedDirty` has run yet.
    if (this->is<Artboard>())
    {
        m_Artboard = this->as<Artboard>();
        return StatusCode::Ok;
    }
    auto* resolved = context->resolve(parentId());
    if (resolved == nullptr || !resolved->is<ContainerComponent>())
    {
        // Parent not in the file (typeKey unknown to this build,
        // or coop hasn't delivered it yet). Tell subclass
        // `onAddedDirty` to bail too — propagating MissingObject
        // means the subclass's `Super::onAddedDirty()` check at
        // the top of every override returns early without
        // calling typed setup like `initPaintMutator` /
        // `parent()->as<X>()->addY(this)`. This mirrors Dart's
        // `validate()` cull (rive_core/lib/component.dart:409 +
        // rive_file.dart:750-764): orphan Cores are simply not
        // wired into the live graph.
        return StatusCode::MissingObject;
    }
    auto* parentContainer = resolved->as<ContainerComponent>();
    setParentForEditor(parentContainer);
    for (Core* curr = resolved; curr != nullptr;)
    {
        if (curr->is<Artboard>())
        {
            m_Artboard = curr->as<Artboard>();
            break;
        }
        if (!curr->is<Component>())
        {
            break;
        }
        curr = context->resolve(curr->as<Component>()->parentId());
    }
    if (m_Artboard == nullptr)
    {
        // Parent chain broken before reaching the Artboard.
        // Same propagation as no-parent: subclass `onAddedDirty`
        // overrides bail on MissingObject from Super, so
        // typed setup like `initPaintMutator(this)` (which derefs
        // `artboard()->factory()`) never runs and never crashes.
        // Pass G in `EditorFile::finalizeBatch` re-resolves the
        // chain on every batch; if a subsequent coop batch fills
        // the missing ancestor, `m_Artboard` gets populated and
        // a re-validation pass (Slice 6 follow-up) can re-run
        // the typed setup.
        setParentForEditor(nullptr);
        return StatusCode::MissingObject;
    }
    parentContainer->addChild(this);
    return StatusCode::Ok;
#else
    m_Artboard = static_cast<Artboard*>(context);
    if (this == m_Artboard)
    {
        // We're the artboard, don't parent to ourselves.
        return StatusCode::Ok;
    }
    m_Parent = context->resolve(parentId())->as<ContainerComponent>();
    m_Parent->addChild(this);
    return StatusCode::Ok;
#endif
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